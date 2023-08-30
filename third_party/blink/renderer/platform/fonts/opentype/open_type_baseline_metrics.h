// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_OPENTYPE_OPEN_TYPE_BASELINE_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_OPENTYPE_OPEN_TYPE_BASELINE_METRICS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

#include <hb.h>

namespace blink {
class HarfBuzzFace;

class PLATFORM_EXPORT OpenTypeBaselineMetrics {
 public:
  OpenTypeBaselineMetrics(HarfBuzzFace* harf_buzz_face,
                          FontOrientation orientation);

  // OpenType spec reference:
  // https://learn.microsoft.com/en-us/typography/opentype/spec/baselinetags
  // Read the alphabetic baseline from the open type table.
  absl::optional<float> OpenTypeAlphabeticBaseline();
  // Read the hanging baseline from the open type table.
  absl::optional<float> OpenTypeHangingBaseline();
  // Read the ideographic baseline from the open type table.
  absl::optional<float> OpenTypeIdeographicBaseline();

 private:
  hb_font_t* font_;
  hb_direction_t hb_dir_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_OPENTYPE_OPEN_TYPE_BASELINE_METRICS_H_
