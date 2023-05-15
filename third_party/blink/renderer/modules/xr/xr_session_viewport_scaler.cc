// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_session_viewport_scaler.h"

#include <algorithm>
#include <cmath>

namespace blink {

namespace {

// Minimum and maximum viewport scale factors. The min value is
// additionally clamped by kMinViewportScale in xr_view.cc.
constexpr float kMinScale = 0.25f;
constexpr float kMaxScale = 1.0f;

// With this scale step, the resulting scales include powers of 1/2:
// [1, 0.841, 0.707, 0.595, 0.5, 0.420, 0.354, 0.297, 0.25]
constexpr float kScaleStep = 0.840896415256f;  // sqrt(sqrt(1/2))

// Thresholds for high/low load values to trigger a scale change.
constexpr float kLoadHigh = 1.25f;
constexpr float kLoadLow = 0.9f;

// Maximum change allowed for a single update. Helps avoid glitches for
// outliers.
constexpr float kMaxChange = 0.5f;

// Load average decay value, smaller values are smoother but react
// slower. Higher values react quicker but may oscillate.
// Must be between 0 and 1.
constexpr float kLoadDecay = 0.3f;

// A power of two used to round the floating point value to a certain number
// of significant bits. This ensures that scale values exactly equal the
// appropriate powers of 2 (1, 0.5, 0.25). We don't want rounding errors to
// result in a scale of 0.99999 instead of 1.0 after multiple iterations of
// scaling up and down.
constexpr float kRound = 65536.0f;

}  // namespace

void XRSessionViewportScaler::ResetLoad() {
  gpu_load_ = (kLoadHigh + kLoadLow) / 2;
}

void XRSessionViewportScaler::UpdateRenderingTimeRatio(float new_value) {
  gpu_load_ +=
      std::clamp(kLoadDecay * (new_value - gpu_load_), -kMaxChange, kMaxChange);
  float old_scale = scale_;
  if (gpu_load_ > kLoadHigh && scale_ > kMinScale) {
    scale_ *= kScaleStep;
    scale_ = round(scale_ * kRound) / kRound;
  } else if (gpu_load_ < kLoadLow && scale_ < kMaxScale) {
    scale_ /= kScaleStep;
    scale_ = round(scale_ * kRound) / kRound;
  }
  scale_ = std::clamp(scale_, kMinScale, kMaxScale);
  if (scale_ != old_scale) {
    ResetLoad();
  }
}

}  // namespace blink
