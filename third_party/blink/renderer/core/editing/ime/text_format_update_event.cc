// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/text_format_update_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_text_format_update_event_init.h"
#include "third_party/blink/renderer/core/editing/ime/text_format.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

TextFormatUpdateEvent::TextFormatUpdateEvent(
    const AtomicString& type,
    const TextFormatUpdateEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasTextFormats())
    text_formats_ = initializer->textFormats();
}

TextFormatUpdateEvent::TextFormatUpdateEvent(
    const AtomicString& type,
    HeapVector<Member<TextFormat>>& text_formats)
    : Event(type,
            Bubbles::kNo,
            Cancelable::kYes,
            ComposedMode::kComposed,
            base::TimeTicks::Now()),
      text_formats_(text_formats) {}

TextFormatUpdateEvent* TextFormatUpdateEvent::Create(
    const AtomicString& type,
    const TextFormatUpdateEventInit* initializer) {
  return MakeGarbageCollected<TextFormatUpdateEvent>(type, initializer);
}

TextFormatUpdateEvent::~TextFormatUpdateEvent() = default;

HeapVector<Member<TextFormat>> TextFormatUpdateEvent::getTextFormats() const {
  return text_formats_;
}

const AtomicString& TextFormatUpdateEvent::InterfaceName() const {
  return event_interface_names::kTextFormatUpdateEvent;
}

void TextFormatUpdateEvent::Trace(Visitor* visitor) const {
  visitor->Trace(text_formats_);
  Event::Trace(visitor);
}

}  // namespace blink
