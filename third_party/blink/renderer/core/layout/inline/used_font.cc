// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/used_font.h"

namespace blink {

LayoutUnit UsedFont::FixedAscent(FontBaseline baseline) const {
  if (const auto* font_data = PrimaryFont()) [[likely]] {
    return LayoutUnit(font_data->GetFontMetrics().FloatAscent(baseline) *
                      text_fit_scaling_factor_);
  }
  return LayoutUnit();
}

LayoutUnit UsedFont::FixedDescent() const {
  if (const auto* font_data = PrimaryFont()) [[likely]] {
    return LayoutUnit(
        font_data->GetFontMetrics().FloatDescent(FontBaseline::kAlphabetic) *
        text_fit_scaling_factor_);
  }
  return LayoutUnit();
}

LayoutUnit UsedFont::FixedDescent(FontBaseline baseline) const {
  if (const auto* font_data = PrimaryFont()) [[likely]] {
    return LayoutUnit(font_data->GetFontMetrics().FloatDescent(baseline) *
                      text_fit_scaling_factor_);
  }
  return LayoutUnit();
}

std::optional<float> UsedFont::UnderlineThickness() const {
  if (const auto* font_data = PrimaryFont()) {
    if (auto optional_thickness =
            font_data->GetFontMetrics().UnderlineThickness()) {
      return *optional_thickness * text_fit_scaling_factor_;
    }
  }
  return std::nullopt;
}

}  // namespace blink
