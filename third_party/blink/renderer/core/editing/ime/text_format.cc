// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/text_format.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_text_format_init.h"

namespace blink {

TextFormat::TextFormat(wtf_size_t range_start,
                       wtf_size_t range_end,
                       const String& text_color,
                       const String& background_color,
                       const String& underline_color,
                       const String& underline_style,
                       const String& underline_thickness)
    : range_start_(range_start),
      range_end_(range_end),
      text_color_(text_color),
      background_color_(background_color),
      underline_color_(underline_color),
      underline_style_(underline_style),
      underline_thickness_(underline_thickness) {}

TextFormat* TextFormat::Create(wtf_size_t range_start,
                               wtf_size_t range_end,
                               const String& text_color,
                               const String& background_color,
                               const String& underline_color,
                               const String& underline_style,
                               const String& underline_thickness) {
  return MakeGarbageCollected<TextFormat>(range_start, range_end, text_color,
                                          background_color, underline_color,
                                          underline_style, underline_thickness);
}

TextFormat::TextFormat(const TextFormatInit* dict) {
  if (dict->hasRangeStart())
    range_start_ = dict->rangeStart();

  if (dict->hasRangeEnd())
    range_end_ = dict->rangeEnd();

  if (dict->hasTextColor())
    text_color_ = dict->textColor();

  if (dict->hasBackgroundColor())
    background_color_ = dict->backgroundColor();

  if (dict->hasUnderlineColor())
    underline_color_ = dict->underlineColor();

  if (dict->hasUnderlineStyle())
    underline_style_ = dict->underlineStyle();

  if (dict->hasUnderlineThickness())
    underline_thickness_ = dict->underlineThickness();
}

TextFormat* TextFormat::Create(const TextFormatInit* dict) {
  return MakeGarbageCollected<TextFormat>(dict);
}

wtf_size_t TextFormat::rangeStart() const {
  return range_start_;
}

wtf_size_t TextFormat::rangeEnd() const {
  return range_end_;
}

String TextFormat::textColor() const {
  return text_color_;
}

String TextFormat::backgroundColor() const {
  return background_color_;
}

String TextFormat::underlineColor() const {
  return underline_color_;
}

String TextFormat::underlineStyle() const {
  return underline_style_;
}

String TextFormat::underlineThickness() const {
  return underline_thickness_;
}

}  // namespace blink
