// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/text_decoration_thickness.h"

namespace blink {

TextDecorationThickness::TextDecorationThickness()
    : thickness_(Length::Auto()) {}

TextDecorationThickness::TextDecorationThickness(const Length& length)
    : thickness_(length) {}

TextDecorationThickness::TextDecorationThickness(CSSValueID from_font_keyword) {
  DCHECK_EQ(from_font_keyword, CSSValueID::kFromFont);
  thickness_from_font_ = true;
}

bool TextDecorationThickness::operator==(
    const TextDecorationThickness& other) const {
  return thickness_from_font_ == other.thickness_from_font_ &&
         thickness_ == other.thickness_;
}

}  // namespace blink
