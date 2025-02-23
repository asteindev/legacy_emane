/*
 * Copyright (c) 2013 - Adjacent Link LLC, Bridgewater, New Jersey
 * Copyright (c) 2010 - DRS CenGen, LLC, Columbia, Maryland
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of DRS CenGen, LLC nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "emane/flowcontrolclient.h"

#include <ace/Guard_T.h>
#include <ace/Thread_Mutex.h>
#include <ace/Condition_T.h>

class EMANE::FlowControlClient::Implementation
{
public:
  Implementation(EMANE::UpstreamTransport & transport):
    rTransport_(transport),
    bCancel_{false},
    cond_{mutex_},
    u16TokensAvailable_{}{}

  ~Implementation(){}

  void start()
  {
    ACE_Guard<ACE_Thread_Mutex> m(mutex_);
    
    // send message 
    rTransport_.sendDownstreamControl({Controls::FlowControlControlMessage::create(u16TokensAvailable_)});
  }

  void stop()
  {
    mutex_.acquire();
    
    // set cancel flag
    bCancel_ = true;
    
    // unblock pending request if any
    cond_.signal();

    mutex_.release();
  }

  bool removeToken()
  {
    ACE_Guard<ACE_Thread_Mutex> m(mutex_);
    
    // request status 
    bool bStatus{false};
    
    // wait unitil tokens are available, and not canceled 
    while(u16TokensAvailable_ == 0 && !bCancel_)
      {
        cond_.wait();
      }
 
    // check canceled flag
    if(bCancel_)
      {
        // failure
        bStatus = false;
      }
    else
      {
      // success
        bStatus = true;
        
        // remove token 
        --u16TokensAvailable_;
      }
    
    return bStatus;
  }

  void processFlowControlMessage(const Controls::FlowControlControlMessage * pMessage)
  {
    ACE_Guard<ACE_Thread_Mutex> m(mutex_);

    u16TokensAvailable_ = pMessage->getTokens();

    rTransport_.sendDownstreamControl({Controls::FlowControlControlMessage::create(u16TokensAvailable_)});

    // unblock pending request if any
    cond_.signal();     
  }

private:
  EMANE::UpstreamTransport & rTransport_;
  
  bool bCancel_;
  
  ACE_Thread_Mutex mutex_;
  
  ACE_Condition<ACE_Thread_Mutex> cond_;
  
  std::uint16_t u16TokensAvailable_;
};

EMANE::FlowControlClient::FlowControlClient(EMANE::UpstreamTransport & transport):
  pImpl_{new Implementation{transport}}
{}



EMANE::FlowControlClient::~FlowControlClient()
{}


void EMANE::FlowControlClient::start()
{
  pImpl_->start();
}


void EMANE::FlowControlClient::stop()
{
  pImpl_->stop();
}



bool EMANE::FlowControlClient::removeToken()
{
  return pImpl_->removeToken();
}


void EMANE::FlowControlClient::processFlowControlMessage(const Controls::FlowControlControlMessage * pMessage)
{
  return  pImpl_->processFlowControlMessage(pMessage);
}
 
