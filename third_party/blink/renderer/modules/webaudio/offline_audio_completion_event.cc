/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/offline_audio_completion_event.h"

#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

OfflineAudioCompletionEvent* OfflineAudioCompletionEvent::Create() {
  return MakeGarbageCollected<OfflineAudioCompletionEvent>();
}

OfflineAudioCompletionEvent* OfflineAudioCompletionEvent::Create(
    AudioBuffer* rendered_buffer) {
  return MakeGarbageCollected<OfflineAudioCompletionEvent>(rendered_buffer);
}

OfflineAudioCompletionEvent* OfflineAudioCompletionEvent::Create(
    const AtomicString& event_type,
    const OfflineAudioCompletionEventInit* event_init) {
  return MakeGarbageCollected<OfflineAudioCompletionEvent>(event_type,
                                                           event_init);
}

OfflineAudioCompletionEvent::OfflineAudioCompletionEvent() = default;

OfflineAudioCompletionEvent::OfflineAudioCompletionEvent(
    AudioBuffer* rendered_buffer)
    : Event(event_type_names::kComplete, Bubbles::kYes, Cancelable::kNo),
      rendered_buffer_(rendered_buffer) {}

OfflineAudioCompletionEvent::OfflineAudioCompletionEvent(
    const AtomicString& event_type,
    const OfflineAudioCompletionEventInit* event_init)
    : Event(event_type, event_init) {
  rendered_buffer_ = event_init->renderedBuffer();
}

OfflineAudioCompletionEvent::~OfflineAudioCompletionEvent() = default;

const AtomicString& OfflineAudioCompletionEvent::InterfaceName() const {
  return event_interface_names::kOfflineAudioCompletionEvent;
}

void OfflineAudioCompletionEvent::Trace(Visitor* visitor) const {
  visitor->Trace(rendered_buffer_);
  Event::Trace(visitor);
}

}  // namespace blink
