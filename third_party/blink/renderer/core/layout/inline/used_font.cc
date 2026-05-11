// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/used_font.h"

namespace blink {

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
