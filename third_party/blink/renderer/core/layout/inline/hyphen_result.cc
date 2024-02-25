// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/hyphen_result.h"

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"

namespace blink {

void HyphenResult::Shape(const ComputedStyle& style) {
  text_ = style.HyphenString();
  HarfBuzzShaper shaper(text_);
  shape_result_ = shaper.Shape(&style.GetFont(), style.Direction());
}

}  // namespace blink
