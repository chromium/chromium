// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_metrics.h"

#import <Accelerate/Accelerate.h>

#import <algorithm>
#import <cmath>

namespace ttc {

float CalculateRMS(base::span<const float> samples) {
  if (samples.empty()) {
    return 0.0f;
  }
  float rms = 0.0f;
  // vDSP_rmsqv computes the root-mean-square of single-precision floating-point
  // vector elements using hardware SIMD vector units.
  vDSP_rmsqv(samples.data(), 1, &rms, samples.size());
  if (!std::isfinite(rms)) {
    return 0.0f;
  }
  return std::clamp(rms, 0.0f, 1.0f);
}

float LinearRmsToPerceptualLevel(float rms) {
  if (rms <= 0.0f || !std::isfinite(rms)) {
    return 0.0f;
  }
  // Convert linear amplitude to decibels relative to full scale (dBFS):
  // dBFS = 20 * log10(rms).
  float db = 20.0f * std::log10(rms);
  if (db <= kMinDecibels) {
    return 0.0f;
  }
  if (db >= kMaxDecibels) {
    return 1.0f;
  }
  // Linearly map the logarithmic range [kMinDecibels, kMaxDecibels] to
  // normalized [0.0, 1.0] for human-perceptual energy representation.
  return (db - kMinDecibels) / (kMaxDecibels - kMinDecibels);
}

}  // namespace ttc
