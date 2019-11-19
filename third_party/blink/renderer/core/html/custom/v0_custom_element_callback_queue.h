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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_CALLBACK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_CALLBACK_QUEUE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_step.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// FIXME: Rename this because it contains resolution and upgrade as
// well as callbacks.
class V0CustomElementCallbackQueue
    : public GarbageCollected<V0CustomElementCallbackQueue> {
 public:
  explicit V0CustomElementCallbackQueue(Element*);

  typedef int ElementQueueId;
  ElementQueueId Owner() const { return owner_; }

  void SetOwner(ElementQueueId new_owner) {
    // ElementCallbackQueues only migrate towards the top of the
    // processing stack.
    DCHECK_GE(new_owner, owner_);
    owner_ = new_owner;
  }

  bool ProcessInElementQueue(ElementQueueId);

  void Append(V0CustomElementProcessingStep* invocation) {
    queue_.push_back(invocation);
  }
  bool InCreatedCallback() const { return in_created_callback_; }

  void Trace(Visitor*);

 private:
  Member<Element> element_;
  HeapVector<Member<V0CustomElementProcessingStep>> queue_;
  ElementQueueId owner_;
  wtf_size_t index_;
  bool in_created_callback_;

  DISALLOW_COPY_AND_ASSIGN(V0CustomElementCallbackQueue);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_CALLBACK_QUEUE_H_
