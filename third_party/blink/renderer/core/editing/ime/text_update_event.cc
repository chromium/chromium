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
  if (initializer->hasUpdateText())
    update_text_ = initializer->updateText();

  if (initializer->hasUpdateRangeStart())
    update_range_start_ = initializer->updateRangeStart();

  if (initializer->hasUpdateRangeEnd())
    update_range_end_ = initializer->updateRangeEnd();

  if (initializer->hasNewSelectionStart())
    new_selection_start_ = initializer->newSelectionStart();

  if (initializer->hasNewSelectionEnd())
    new_selection_end_ = initializer->newSelectionEnd();
}

TextUpdateEvent::TextUpdateEvent(const AtomicString& type,
                                 const String& update_text,
                                 uint32_t update_range_start,
                                 uint32_t update_range_end,
                                 uint32_t new_selection_start,
                                 uint32_t new_selection_end)
    : Event(type, Bubbles::kNo, Cancelable::kYes, ComposedMode::kComposed),
      update_text_(update_text),
      update_range_start_(update_range_start),
      update_range_end_(update_range_end),
      new_selection_start_(new_selection_start),
      new_selection_end_(new_selection_end) {}

TextUpdateEvent* TextUpdateEvent::Create(
    const AtomicString& type,
    const TextUpdateEventInit* initializer) {
  return MakeGarbageCollected<TextUpdateEvent>(type, initializer);
}

TextUpdateEvent::~TextUpdateEvent() = default;

String TextUpdateEvent::updateText() const {
  return update_text_;
}

uint32_t TextUpdateEvent::updateRangeStart() const {
  return update_range_start_;
}

uint32_t TextUpdateEvent::updateRangeEnd() const {
  return update_range_end_;
}

uint32_t TextUpdateEvent::newSelectionStart() const {
  return new_selection_start_;
}
uint32_t TextUpdateEvent::newSelectionEnd() const {
  return new_selection_end_;
}

const AtomicString& TextUpdateEvent::InterfaceName() const {
  return event_interface_names::kTextUpdateEvent;
}

}  // namespace blink
