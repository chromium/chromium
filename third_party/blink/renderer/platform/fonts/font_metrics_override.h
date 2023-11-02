// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_METRICS_OVERRIDE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_METRICS_OVERRIDE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {

struct FontMetricsOverride {
  absl::optional<float> ascent_override;
  absl::optional<float> descent_override;
  absl::optional<float> line_gap_override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_METRICS_OVERRIDE_H_
