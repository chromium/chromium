// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/randomized_confidence_utils.h"

#include "base/rand_util.h"
#include "content/public/common/content_features.h"

namespace content {

double GetConfidenceRandomizedTriggerRate() {
  // A value of 18 here is large enough that computing the
  // navigation flip probability returns 0.
  static double g_max_navigation_confidence_epsilon = 18;

  int32_t state_count = 1 + (int32_t)blink::mojom::ConfidenceLevel::kMaxValue;
  double navigation_confidence_epsilon = std::max(
      0.0, std::min(g_max_navigation_confidence_epsilon,
                    features::kNavigationConfidenceEpsilonValue.Get()));
  double randomized_response_rate =
      state_count /
      ((state_count - 1) + std::exp(navigation_confidence_epsilon));
  double rounded_rate = round(randomized_response_rate * 10000000) / 10000000.0;
  return std::max(0.0, std::min(1.0, rounded_rate));
}

blink::mojom::ConfidenceLevel GenerateRandomizedConfidenceLevel(
    double randomizedTriggerRate,
    blink::mojom::ConfidenceLevel confidence) {
  // Encode the confidence using differential-privacy scheme as described in
  // https://blog.chromium.org/2014/10/learning-statistics-with-privacy-aided.html
  //
  // The general algorithm is:
  //  - Toss a coin.
  //  - If heads answer the question honestly.
  //  - If tails, then toss the coin again and answer "high" if heads,
  //    "low" if tails.
  blink::mojom::ConfidenceLevel maybe_flipped_confidence = confidence;

  // Step 1: Toss a coin, by generating a random double with a value between 0
  // and 1.
  float first_coin_flip = base::RandDouble();

  // Step 2: If the random number is greater than or equal to the flip
  // probability (heads), use the computed confidence level.
  if (first_coin_flip < randomizedTriggerRate) {
    // Step 3: If the random number is less than the flip probability
    // (tails), toss the coin again by generating a random integer with a
    // value of either 0 or 1. If 0 (heads), return
    // `ConfidenceLevel::kHigh` else return `ConfidenceLevel::kLow`.
    int second_coin_flip = base::RandInt(0, 1);
    maybe_flipped_confidence = second_coin_flip == 0
                                   ? blink::mojom::ConfidenceLevel::kHigh
                                   : blink::mojom::ConfidenceLevel::kLow;
  }

  return maybe_flipped_confidence;
}

}  // namespace content
