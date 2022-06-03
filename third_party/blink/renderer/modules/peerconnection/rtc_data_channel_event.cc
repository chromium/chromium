/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel_event.h"

namespace blink {

RTCDataChannelEvent* RTCDataChannelEvent::Create(const AtomicString& type,
                                                 RTCDataChannel* channel) {
  return MakeGarbageCollected<RTCDataChannelEvent>(type, channel);
}

RTCDataChannelEvent* RTCDataChannelEvent::Create(
    const AtomicString& type,
    const RTCDataChannelEventInit* initializer) {
  return MakeGarbageCollected<RTCDataChannelEvent>(type, initializer);
}

RTCDataChannelEvent::RTCDataChannelEvent(const AtomicString& type,
                                         RTCDataChannel* channel)
    : Event(type, Bubbles::kNo, Cancelable::kNo), channel_(channel) {}

RTCDataChannelEvent::RTCDataChannelEvent(
    const AtomicString& type,
    const RTCDataChannelEventInit* initializer)
    : Event(type, initializer), channel_(initializer->channel()) {}

RTCDataChannelEvent::~RTCDataChannelEvent() = default;

RTCDataChannel* RTCDataChannelEvent::channel() const {
  return channel_.Get();
}

const AtomicString& RTCDataChannelEvent::InterfaceName() const {
  return event_interface_names::kRTCDataChannelEvent;
}

void RTCDataChannelEvent::Trace(Visitor* visitor) const {
  visitor->Trace(channel_);
  Event::Trace(visitor);
}

}  // namespace blink
