// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/text_decoration_thickness.h"

#include <cmath>
#include <optional>

#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

TextDecorationThickness::TextDecorationThickness()
    : thickness_(Length::Auto()) {}

TextDecorationThickness::TextDecorationThickness(const Length& length)
    : thickness_(length) {}

TextDecorationThickness::TextDecorationThickness(CSSValueID from_font_keyword) {
  DCHECK_EQ(from_font_keyword, CSSValueID::kFromFont);
  thickness_from_font_ = true;
}

float TextDecorationThickness::Resolve(float font_size,
                                       const SimpleFontData* primary_font,
                                       float font_scale) const {
  const float auto_thickness = font_size / 10.0f;
  if (IsAuto() || !primary_font) {
    return auto_thickness;
  }
  if (IsFromFont()) {
    const std::optional<float> underline_thickness =
        primary_font->GetFontMetrics().UnderlineThickness();
    return underline_thickness ? *underline_thickness * font_scale
                               : auto_thickness;
  }
  return std::round(FloatValueForLength(Thickness(), font_size));
}

bool TextDecorationThickness::operator==(
    const TextDecorationThickness& other) const {
  return thickness_from_font_ == other.thickness_from_font_ &&
         thickness_ == other.thickness_;
}

}  // namespace blink
