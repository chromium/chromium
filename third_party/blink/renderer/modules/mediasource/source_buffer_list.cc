/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "third_party/blink/renderer/modules/mediasource/source_buffer_list.h"

#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer.h"

namespace blink {

SourceBufferList::SourceBufferList(ExecutionContext* context,
                                   EventQueue* async_event_queue)
    : ExecutionContextClient(context), async_event_queue_(async_event_queue) {}

SourceBufferList::~SourceBufferList() = default;

void SourceBufferList::Add(SourceBuffer* buffer) {
  list_.push_back(buffer);
  ScheduleEvent(event_type_names::kAddsourcebuffer);
}

void SourceBufferList::insert(wtf_size_t position, SourceBuffer* buffer) {
  list_.insert(position, buffer);
  ScheduleEvent(event_type_names::kAddsourcebuffer);
}

void SourceBufferList::Remove(SourceBuffer* buffer) {
  wtf_size_t index = list_.Find(buffer);
  if (index == kNotFound)
    return;
  list_.EraseAt(index);
  ScheduleEvent(event_type_names::kRemovesourcebuffer);
}

void SourceBufferList::Clear() {
  list_.clear();
  ScheduleEvent(event_type_names::kRemovesourcebuffer);
}

void SourceBufferList::ScheduleEvent(const AtomicString& event_name) {
  DCHECK(async_event_queue_);

  Event* event = Event::Create(event_name);
  event->SetTarget(this);

  async_event_queue_->EnqueueEvent(FROM_HERE, *event);
}

const AtomicString& SourceBufferList::InterfaceName() const {
  return event_target_names::kSourceBufferList;
}

void SourceBufferList::Trace(Visitor* visitor) const {
  visitor->Trace(async_event_queue_);
  visitor->Trace(list_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
