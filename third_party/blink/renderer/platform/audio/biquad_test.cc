// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/biquad.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// Sweeps a standard range of parameters (frequency, Q, gain) for all filter
// types to ensure no regressions or crashes under typical/moderate values.
// The check ensures that TailFrame does not return NaN.
TEST(BiquadTailFrameTest, AllFiltersSweep) {
  Biquad biquad(128);

  for (double freq = 0.001; freq < 0.99; freq += 0.05) {
    for (double param = -20.0; param < 20.0; param += 0.1) {
      biquad.SetLowpassParams(0, freq, param);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

      biquad.SetHighpassParams(0, freq, param);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

      biquad.SetPeakingParams(0, freq, 1.0, param);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

      biquad.SetLowShelfParams(0, freq, param);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

      biquad.SetHighShelfParams(0, freq, param);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));
    }

    for (double q = 0.01; q < 100.0; q *= 1.5) {
      biquad.SetBandpassParams(0, freq, q);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

      biquad.SetNotchParams(0, freq, q);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

      biquad.SetAllpassParams(0, freq, q);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));
    }
  }
}

// Sweeps extreme parameters to test the robustness of TailFrame math.
// - Extreme frequencies close to 0 and Nyquist (0.0001, 0.4999).
// - Extreme gain/Q values (e.g. -1000, 1000, 1e300, 1e-300) to trigger
//   underflow, overflow, and negative root scenarios.
// The test verifies that TailFrame safely returns a non-NaN value (finite or
// infinity for unstable filters) and does not crash.
TEST(BiquadTailFrameTest, ExtremeParametersAllFilters) {
  Biquad biquad(128);

  double extreme_params[] = {
      -1000.0, -500.0, -100.0, 100.0,  500.0,  1000.0,
      1e-300,  1e-150, 1e-100, 1e100,  1e150,  1e300};

  for (double freq : {0.0001, 0.001, 0.1, 0.25, 0.49, 0.4999}) {
    for (double param : extreme_params) {
      biquad.SetLowpassParams(0, freq, param);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

      biquad.SetHighpassParams(0, freq, param);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

      if (param > 0) {
        biquad.SetPeakingParams(0, freq, param, 10.0);
        EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));
      }

      biquad.SetPeakingParams(0, freq, 1.0, param);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

      biquad.SetLowShelfParams(0, freq, param);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

      biquad.SetHighShelfParams(0, freq, param);
      EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

      if (param > 0) {
        biquad.SetBandpassParams(0, freq, param);
        EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

        biquad.SetNotchParams(0, freq, param);
        EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));

        biquad.SetAllpassParams(0, freq, param);
        EXPECT_FALSE(std::isnan(biquad.TailFrame(0, 30 * 48000)));
      }
    }
  }
}

}  // namespace blink
