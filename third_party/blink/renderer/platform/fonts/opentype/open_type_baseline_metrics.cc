// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <hb-ot.h>
#include <hb.h>

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_baseline_metrics.h"

#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"

namespace {
// HarfBuzz' hb_position_t is a 16.16 fixed-point value.
float HarfBuzzUnitsToFloat(hb_position_t value) {
  static const float kFloatToHbRatio = 1.0f / (1 << 16);
  return kFloatToHbRatio * value;
}

}  // namespace

namespace blink {
OpenTypeBaselineMetrics::OpenTypeBaselineMetrics(HarfBuzzFace* harf_buzz_face,
                                                 FontOrientation orientation) {
  hb_dir_ =
      IsVerticalBaseline(orientation) ? HB_DIRECTION_TTB : HB_DIRECTION_LTR;
  font_ = harf_buzz_face->GetScaledFont();
}

std::optional<float> OpenTypeBaselineMetrics::OpenTypeAlphabeticBaseline() {
  std::optional<float> result;
  DCHECK(font_);

  hb_position_t position;

  if (hb_ot_layout_get_baseline(font_, HB_OT_LAYOUT_BASELINE_TAG_ROMAN, hb_dir_,
                                HB_OT_TAG_DEFAULT_SCRIPT,
                                HB_OT_TAG_DEFAULT_LANGUAGE, &position)) {
    result = HarfBuzzUnitsToFloat(position);
  }
  return result;
}

std::optional<float> OpenTypeBaselineMetrics::OpenTypeHangingBaseline() {
  std::optional<float> result;
  DCHECK(font_);

  hb_position_t position;

  if (hb_ot_layout_get_baseline(font_, HB_OT_LAYOUT_BASELINE_TAG_HANGING,
                                hb_dir_, HB_OT_TAG_DEFAULT_SCRIPT,
                                HB_OT_TAG_DEFAULT_LANGUAGE, &position)) {
    result = HarfBuzzUnitsToFloat(position);
  }
  return result;
}

std::optional<float> OpenTypeBaselineMetrics::OpenTypeIdeographicBaseline() {
  std::optional<float> result;
  DCHECK(font_);

  hb_position_t position;

  if (hb_ot_layout_get_baseline(
          font_, HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT, hb_dir_,
          HB_OT_TAG_DEFAULT_SCRIPT, HB_OT_TAG_DEFAULT_LANGUAGE, &position)) {
    result = HarfBuzzUnitsToFloat(position);
  }
  return result;
}

}  // namespace blink
