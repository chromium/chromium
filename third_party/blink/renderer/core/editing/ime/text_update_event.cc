// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/text_update_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_text_update_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

TextUpdateEvent::TextUpdateEvent(const TextUpdateEventInit* dict) {
  if (dict->hasUpdateText())
    update_text_ = dict->updateText();

  if (dict->hasUpdateRangeStart())
    update_range_start_ = dict->updateRangeStart();

  if (dict->hasUpdateRangeEnd())
    update_range_end_ = dict->updateRangeEnd();

  if (dict->hasNewSelectionStart())
    new_selection_start_ = dict->newSelectionStart();

  if (dict->hasNewSelectionEnd())
    new_selection_end_ = dict->newSelectionEnd();
}

TextUpdateEvent::TextUpdateEvent(const String& update_text,
                                 uint32_t update_range_start,
                                 uint32_t update_range_end,
                                 uint32_t new_selection_start,
                                 uint32_t new_selection_end)
    : Event(event_type_names::kTextupdate,
            Bubbles::kNo,
            Cancelable::kYes,
            ComposedMode::kComposed,
            base::TimeTicks::Now()),
      update_text_(update_text),
      update_range_start_(update_range_start),
      update_range_end_(update_range_end),
      new_selection_start_(new_selection_start),
      new_selection_end_(new_selection_end) {}

TextUpdateEvent* TextUpdateEvent::Create(const TextUpdateEventInit* dict) {
  return MakeGarbageCollected<TextUpdateEvent>(dict);
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
