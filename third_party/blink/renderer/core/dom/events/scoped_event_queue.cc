/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"

namespace blink {

ScopedEventQueue* ScopedEventQueue::instance_ = nullptr;

ScopedEventQueue::ScopedEventQueue()
    : queued_events_(MakeGarbageCollected<GCedHeapVector<Member<Event>>>()),
      scoping_level_(0) {}

ScopedEventQueue::~ScopedEventQueue() {
  DCHECK(!scoping_level_);
  DCHECK(!queued_events_->size());
}

void ScopedEventQueue::Initialize() {
  DCHECK(!instance_);
  std::unique_ptr<ScopedEventQueue> instance =
      base::WrapUnique(new ScopedEventQueue);
  instance_ = instance.release();
}

void ScopedEventQueue::EnqueueEvent(Event& event) {
  if (ShouldQueueEvents())
    queued_events_->push_back(event);
  else
    DispatchEvent(event);
}

void ScopedEventQueue::DispatchAllEvents() {
  HeapVector<Member<Event>> queued_events;
  queued_events.swap(*queued_events_);

  for (auto& event : queued_events)
    DispatchEvent(*event);
}

void ScopedEventQueue::DispatchEvent(Event& event) const {
  DCHECK(event.RawTarget());
  Node* node = event.RawTarget()->ToNode();
  EventDispatcher::DispatchEvent(*node, event);
}

ScopedEventQueue* ScopedEventQueue::Instance() {
  if (!instance_)
    Initialize();

  return instance_;
}

void ScopedEventQueue::IncrementScopingLevel() {
  scoping_level_++;
}

void ScopedEventQueue::DecrementScopingLevel() {
  DCHECK(scoping_level_);
  scoping_level_--;
  if (!scoping_level_)
    DispatchAllEvents();
}

}  // namespace blink
