// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/hyphen_result.h"

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void HyphenResult::Shape(const ComputedStyle& style) {
  text_ = style.HyphenString();
  if (RuntimeEnabledFeatures::HyphenVerticalOrientationFixEnabled() &&
      style.GetFontDescription().Orientation() ==
          FontOrientation::kVerticalMixed) {
    // HarfBuzzShaper skips run segmentation for 8-bit text, so use 16-bit
    // storage to apply the appropriate orientation.
    text_.Ensure16Bit();
  }
  HarfBuzzShaper shaper(text_);
  shape_result_ = shaper.Shape(style.GetFont(), style.Direction());
}

}  // namespace blink
