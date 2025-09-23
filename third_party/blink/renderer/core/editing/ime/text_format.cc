// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/text_format.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_text_format_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underline_style.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underline_thickness.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

TextFormat::TextFormat(wtf_size_t range_start,
                       wtf_size_t range_end,
                       const String& underline_style,
                       const String& underline_thickness,
                       ExceptionState& exception_state)
    : range_start_(range_start),
      range_end_(range_end),
      underline_style_(underline_style),
      underline_thickness_(underline_thickness) {}

TextFormat* TextFormat::Create(wtf_size_t range_start,
                               wtf_size_t range_end,
                               const String& underline_style,
                               const String& underline_thickness,
                               ExceptionState& exception_state) {
  return MakeGarbageCollected<TextFormat>(range_start, range_end,
                                          underline_style, underline_thickness,
                                          exception_state);
}

TextFormat::TextFormat(const TextFormatInit* dict,
                       ExceptionState& exception_state) {
  if (dict->hasRangeStart())
    range_start_ = dict->rangeStart();

  if (dict->hasRangeEnd())
    range_end_ = dict->rangeEnd();

  const bool use_spec_values = RuntimeEnabledFeatures::
      UseSpecValuesInTextFormatUpdateEventStylesEnabled();

  if (dict->hasUnderlineStyle()) {
    String style = dict->underlineStyle();
    if (use_spec_values && !V8UnderlineStyle::Create(style)) {
      // Value was invalid, throw error.
      StringBuilder error_message;
      error_message.Append(
          "Failed to read the 'underlineStyle' property from 'TextFormatInit': "
          "The provided value '");
      error_message.Append(style);
      error_message.Append(
          "' is not a valid enum value of type UnderlineStyle.");
      exception_state.ThrowTypeError(error_message.ToString());
      return;
    }
    underline_style_ = style;
  } else if (use_spec_values) {
    underline_style_ = "none";
  }

  if (dict->hasUnderlineThickness()) {
    String thickness = dict->underlineThickness();
    if (use_spec_values && !V8UnderlineThickness::Create(thickness)) {
      // Value was invalid, throw error.
      StringBuilder error_message;
      error_message.Append(
          "Failed to read the 'underlineThickness' property from "
          "'TextFormatInit': The provided value '");
      error_message.Append(thickness);
      error_message.Append(
          "' is not a valid enum value of type UnderlineThickness.");
      exception_state.ThrowTypeError(error_message.ToString());
      return;
    }
    underline_thickness_ = thickness;
  } else if (use_spec_values) {
    underline_thickness_ = "none";
  }
}

TextFormat* TextFormat::Create(const TextFormatInit* dict,
                               ExceptionState& exception_state) {
  return MakeGarbageCollected<TextFormat>(dict, exception_state);
}

wtf_size_t TextFormat::rangeStart() const {
  return range_start_;
}

wtf_size_t TextFormat::rangeEnd() const {
  return range_end_;
}

String TextFormat::underlineStyle() const {
  return underline_style_;
}

String TextFormat::underlineThickness() const {
  return underline_thickness_;
}

}  // namespace blink
