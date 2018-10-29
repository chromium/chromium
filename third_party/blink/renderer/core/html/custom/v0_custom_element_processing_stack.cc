/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_callback_queue.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_scheduler.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

wtf_size_t V0CustomElementProcessingStack::element_queue_start_ = 0;

// The base of the stack has a null sentinel value.
wtf_size_t V0CustomElementProcessingStack::element_queue_end_ = kNumSentinels;

V0CustomElementProcessingStack& V0CustomElementProcessingStack::Instance() {
  DEFINE_STATIC_LOCAL(Persistent<V0CustomElementProcessingStack>, instance,
                      (new V0CustomElementProcessingStack));
  return *instance;
}

// Dispatches callbacks when popping the processing stack.
void V0CustomElementProcessingStack::ProcessElementQueueAndPop() {
  Instance().ProcessElementQueueAndPop(element_queue_start_,
                                       element_queue_end_);
}

void V0CustomElementProcessingStack::ProcessElementQueueAndPop(wtf_size_t start,
                                                               wtf_size_t end) {
  DCHECK(IsMainThread());
  V0CustomElementCallbackQueue::ElementQueueId this_queue =
      CurrentElementQueue();

  for (wtf_size_t i = start; i < end; ++i) {
    {
      // The created callback may schedule entered document
      // callbacks.
      CallbackDeliveryScope delivery_scope;
      flattened_processing_stack_[i]->ProcessInElementQueue(this_queue);
    }

    DCHECK_EQ(start, element_queue_start_);
    DCHECK_EQ(end, element_queue_end_);
  }

  // Pop the element queue from the processing stack
  flattened_processing_stack_.resize(start);
  element_queue_end_ = start;

  if (element_queue_start_ == kNumSentinels)
    V0CustomElementScheduler::CallbackDispatcherDidFinish();
}

void V0CustomElementProcessingStack::Enqueue(
    V0CustomElementCallbackQueue* callback_queue) {
  DCHECK(InCallbackDeliveryScope());

  if (callback_queue->Owner() == CurrentElementQueue())
    return;

  callback_queue->SetOwner(CurrentElementQueue());

  flattened_processing_stack_.push_back(callback_queue);
  ++element_queue_end_;
}

void V0CustomElementProcessingStack::Trace(blink::Visitor* visitor) {
  visitor->Trace(flattened_processing_stack_);
}

}  // namespace blink
