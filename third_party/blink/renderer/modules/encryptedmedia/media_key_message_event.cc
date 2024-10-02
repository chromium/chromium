/*
 * Copyright (C) 2012 Google Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/encryptedmedia/media_key_message_event.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

namespace blink {

MediaKeyMessageEvent::MediaKeyMessageEvent() = default;

MediaKeyMessageEvent::MediaKeyMessageEvent(
    const AtomicString& type,
    const MediaKeyMessageEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasMessageType())
    message_type_ = String(initializer->messageType());
  if (initializer->hasMessage())
    message_ = initializer->message();
}

MediaKeyMessageEvent::~MediaKeyMessageEvent() = default;

const AtomicString& MediaKeyMessageEvent::InterfaceName() const {
  return event_interface_names::kMediaKeyMessageEvent;
}

void MediaKeyMessageEvent::Trace(Visitor* visitor) const {
  visitor->Trace(message_);
  Event::Trace(visitor);
}

}  // namespace blink
