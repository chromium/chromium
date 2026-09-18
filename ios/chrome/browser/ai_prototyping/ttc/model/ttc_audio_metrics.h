// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_METRICS_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_METRICS_H_

#import "base/containers/span.h"

namespace ttc {

// Decibel floor below which ambient room noise or whispering is suppressed.
// Speech typically lies between -40 dBFS and -16 dBFS.
inline constexpr float kMinDecibels = -45.0f;

// Maximum decibel ceiling representing 0 dBFS peak / full scale clipping.
inline constexpr float kMaxDecibels = 0.0f;

// Calculates root-mean-square (RMS) energy across a buffer of Float32 audio
// samples using Apple Accelerate framework's vector processing (`vDSP_rmsqv`).
// Returns a linear amplitude clamped to [0.0, 1.0]. Returns 0.0 if `samples` is
// empty or if non-finite/NaN values are detected.
float CalculateRMS(base::span<const float> samples);

// Converts a linear RMS amplitude value to a human-perceptual audio level
// scaled linearly in the decibel domain [kMinDecibels, kMaxDecibels] to
// [0.0, 1.0]. Values at or below `kMinDecibels` (-45 dBFS) are clamped to 0.0,
// and values at or above `kMaxDecibels` (0 dBFS) are clamped to 1.0. Non-finite
// and negative inputs return 0.0.
float LinearRmsToPerceptualLevel(float rms);

}  // namespace ttc

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_METRICS_H_
