// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/text_update_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_text_update_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

TextUpdateEvent::TextUpdateEvent(const AtomicString& type,
                                 const TextUpdateEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasText()) {
    text_ = initializer->text();
  }

  if (initializer->hasUpdateRangeStart())
    update_range_start_ = initializer->updateRangeStart();

  if (initializer->hasUpdateRangeEnd())
    update_range_end_ = initializer->updateRangeEnd();

  if (initializer->hasSelectionStart()) {
    selection_start_ = initializer->selectionStart();
  }

  if (initializer->hasSelectionEnd()) {
    selection_end_ = initializer->selectionEnd();
  }
}

TextUpdateEvent::TextUpdateEvent(const AtomicString& type,
                                 const String& text,
                                 uint32_t update_range_start,
                                 uint32_t update_range_end,
                                 uint32_t selection_start,
                                 uint32_t selection_end)
    : Event(type, Bubbles::kNo, Cancelable::kYes, ComposedMode::kComposed),
      text_(text),
      update_range_start_(update_range_start),
      update_range_end_(update_range_end),
      selection_start_(selection_start),
      selection_end_(selection_end) {}

TextUpdateEvent* TextUpdateEvent::Create(
    const AtomicString& type,
    const TextUpdateEventInit* initializer) {
  return MakeGarbageCollected<TextUpdateEvent>(type, initializer);
}

TextUpdateEvent::~TextUpdateEvent() = default;

String TextUpdateEvent::text() const {
  return text_;
}

uint32_t TextUpdateEvent::updateRangeStart() const {
  return update_range_start_;
}

uint32_t TextUpdateEvent::updateRangeEnd() const {
  return update_range_end_;
}

uint32_t TextUpdateEvent::selectionStart() const {
  return selection_start_;
}
uint32_t TextUpdateEvent::selectionEnd() const {
  return selection_end_;
}

const AtomicString& TextUpdateEvent::InterfaceName() const {
  return event_interface_names::kTextUpdateEvent;
}

}  // namespace blink
