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

#include "third_party/blink/renderer/core/css/font_face_set_load_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_set_load_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

FontFaceSetLoadEvent::FontFaceSetLoadEvent(const AtomicString& type,
                                           const FontFaceArray& fontfaces)
    : Event(type, Bubbles::kNo, Cancelable::kNo), fontfaces_(fontfaces) {}

FontFaceSetLoadEvent::FontFaceSetLoadEvent(
    const AtomicString& type,
    const FontFaceSetLoadEventInit* initializer)
    : Event(type, initializer), fontfaces_(initializer->fontfaces()) {}

FontFaceSetLoadEvent::~FontFaceSetLoadEvent() = default;

const AtomicString& FontFaceSetLoadEvent::InterfaceName() const {
  return event_interface_names::kFontFaceSetLoadEvent;
}

void FontFaceSetLoadEvent::Trace(Visitor* visitor) const {
  visitor->Trace(fontfaces_);
  Event::Trace(visitor);
}

}  // namespace blink
