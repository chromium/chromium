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

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_callback_queue.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"

namespace blink {

V0CustomElementCallbackQueue::V0CustomElementCallbackQueue(Element* element)
    : element_(element), owner_(-1), index_(0), in_created_callback_(false) {}

bool V0CustomElementCallbackQueue::ProcessInElementQueue(
    ElementQueueId caller) {
  DCHECK(!in_created_callback_);
  bool did_work = false;

  // Never run custom element callbacks in UA shadow roots since that would
  // leak the UA root and it's elements into the page.
  ShadowRoot* shadow_root = element_->ContainingShadowRoot();
  if (!shadow_root || !shadow_root->IsUserAgent()) {
    while (index_ < queue_.size() && Owner() == caller) {
      in_created_callback_ = queue_[index_]->IsCreatedCallback();

      // dispatch() may cause recursion which steals this callback
      // queue and reenters processInQueue. owner() == caller
      // detects this recursion and cedes processing.
      queue_[index_++]->Dispatch(element_.Get());
      in_created_callback_ = false;
      did_work = true;
    }
  }

  if (Owner() == caller && index_ == queue_.size()) {
    // This processInQueue exhausted the queue; shrink it.
    index_ = 0;
    queue_.resize(0);
    owner_ = -1;
  }

  return did_work;
}

void V0CustomElementCallbackQueue::Trace(Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(queue_);
}

}  // namespace blink
