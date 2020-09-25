// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/text_format_update_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_text_format_update_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

TextFormatUpdateEvent::TextFormatUpdateEvent(
    const TextFormatUpdateEventInit* dict) {
  if (dict->hasFormatRangeStart())
    format_range_start_ = dict->formatRangeStart();

  if (dict->hasFormatRangeEnd())
    format_range_end_ = dict->formatRangeEnd();

  if (dict->hasUnderlineColor())
    underline_color_ = dict->underlineColor();

  if (dict->hasBackgroundColor())
    background_color_ = dict->backgroundColor();

  if (dict->hasSuggestionHighlightColor())
    suggestion_highlight_color_ = dict->suggestionHighlightColor();

  if (dict->hasTextColor())
    text_color_ = dict->textColor();

  if (dict->hasUnderlineThickness())
    underline_thickness_ = dict->underlineThickness();

  if (dict->hasUnderlineStyle())
    underline_style_ = dict->underlineStyle();
}

TextFormatUpdateEvent::TextFormatUpdateEvent(
    uint32_t format_range_start,
    uint32_t format_range_end,
    const String& underline_color,
    const String& background_color,
    const String& suggestion_highlight_color,
    const String& text_color,
    const String& underline_thickness,
    const String& underline_style)
    : Event(event_type_names::kTextformatupdate,
            Bubbles::kNo,
            Cancelable::kYes,
            ComposedMode::kComposed,
            base::TimeTicks::Now()),
      format_range_start_(format_range_start),
      format_range_end_(format_range_end),
      underline_color_(underline_color),
      background_color_(background_color),
      suggestion_highlight_color_(suggestion_highlight_color),
      text_color_(text_color),
      underline_thickness_(underline_thickness),
      underline_style_(underline_style) {}

TextFormatUpdateEvent* TextFormatUpdateEvent::Create(
    const TextFormatUpdateEventInit* dict) {
  return MakeGarbageCollected<TextFormatUpdateEvent>(dict);
}

TextFormatUpdateEvent::~TextFormatUpdateEvent() = default;

uint32_t TextFormatUpdateEvent::formatRangeStart() const {
  return format_range_start_;
}

uint32_t TextFormatUpdateEvent::formatRangeEnd() const {
  return format_range_end_;
}

String TextFormatUpdateEvent::underlineColor() const {
  return underline_color_;
}

String TextFormatUpdateEvent::backgroundColor() const {
  return background_color_;
}

String TextFormatUpdateEvent::suggestionHighlightColor() const {
  return suggestion_highlight_color_;
}

String TextFormatUpdateEvent::textColor() const {
  return text_color_;
}

String TextFormatUpdateEvent::underlineThickness() const {
  return underline_thickness_;
}

String TextFormatUpdateEvent::underlineStyle() const {
  return underline_style_;
}

const AtomicString& TextFormatUpdateEvent::InterfaceName() const {
  return event_interface_names::kTextFormatUpdateEvent;
}

}  // namespace blink
