/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/peerconnection/rtc_dtmf_tone_change_event.h"

#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

RTCDTMFToneChangeEvent* RTCDTMFToneChangeEvent::Create(const String& tone) {
  return MakeGarbageCollected<RTCDTMFToneChangeEvent>(tone);
}

RTCDTMFToneChangeEvent* RTCDTMFToneChangeEvent::Create(
    const AtomicString& type,
    const RTCDTMFToneChangeEventInit* initializer) {
  return MakeGarbageCollected<RTCDTMFToneChangeEvent>(initializer);
}

RTCDTMFToneChangeEvent::RTCDTMFToneChangeEvent(const String& tone)
    : Event(event_type_names::kTonechange, Bubbles::kNo, Cancelable::kNo),
      tone_(tone) {}

RTCDTMFToneChangeEvent::RTCDTMFToneChangeEvent(
    const RTCDTMFToneChangeEventInit* initializer)
    : Event(event_type_names::kTonechange, initializer) {
  if (initializer->hasTone())
    tone_ = initializer->tone();
}

RTCDTMFToneChangeEvent::~RTCDTMFToneChangeEvent() = default;

const String& RTCDTMFToneChangeEvent::tone() const {
  return tone_;
}

const AtomicString& RTCDTMFToneChangeEvent::InterfaceName() const {
  return event_interface_names::kRTCDTMFToneChangeEvent;
}

void RTCDTMFToneChangeEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
