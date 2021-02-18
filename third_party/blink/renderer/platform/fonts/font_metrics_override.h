// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_METRICS_OVERRIDE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_METRICS_OVERRIDE_H_

#include "base/optional.h"

namespace blink {

struct FontMetricsOverride {
  base::Optional<float> ascent_override;
  base::Optional<float> descent_override;
  base::Optional<float> line_gap_override;
  base::Optional<float> advance_override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_METRICS_OVERRIDE_H_
