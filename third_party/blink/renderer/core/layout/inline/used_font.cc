// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/used_font.h"

namespace blink {

float UsedFont::FloatAscent() const {
  if (const auto* font_data = PrimaryFont()) {
    return font_data->GetFontMetrics().FloatAscent() * text_fit_scaling_factor_;
  }
  return 0.0f;
}

}  // namespace blink
