// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/text_format.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_text_format_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underline_style.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underline_thickness.h"

namespace blink {

TextFormat::TextFormat(wtf_size_t range_start,
                       wtf_size_t range_end,
                       V8UnderlineStyle::Enum underline_style,
                       V8UnderlineThickness::Enum underline_thickness)
    : range_start_(range_start),
      range_end_(range_end),
      underline_style_(underline_style),
      underline_thickness_(underline_thickness) {}

TextFormat* TextFormat::Create(wtf_size_t range_start,
                               wtf_size_t range_end,
                               V8UnderlineStyle::Enum underline_style,
                               V8UnderlineThickness::Enum underline_thickness) {
  return MakeGarbageCollected<TextFormat>(range_start, range_end,
                                          underline_style, underline_thickness);
}

TextFormat::TextFormat(const TextFormatInit* dict) {
  if (dict->hasRangeStart())
    range_start_ = dict->rangeStart();

  if (dict->hasRangeEnd())
    range_end_ = dict->rangeEnd();

  if (dict->hasUnderlineStyle()) {
    underline_style_ = dict->underlineStyle().AsEnum();
  }

  if (dict->hasUnderlineThickness()) {
    underline_thickness_ = dict->underlineThickness().AsEnum();
  }
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

V8UnderlineStyle TextFormat::underlineStyle() const {
  return V8UnderlineStyle(underline_style_);
}

V8UnderlineThickness TextFormat::underlineThickness() const {
  return V8UnderlineThickness(underline_thickness_);
}

}  // namespace blink
