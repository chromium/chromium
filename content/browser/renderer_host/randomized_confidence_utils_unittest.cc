// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/randomized_confidence_utils.h"

#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

bool IsComputedConfidenceInExpectedRange(blink::mojom::ConfidenceLevel value) {
  return value == blink::mojom::ConfidenceLevel::kHigh ||
         value == blink::mojom::ConfidenceLevel::kLow;
}

bool IsRandomizedTriggerRateInExpectedRange(double value) {
  return 0.0 <= value && value <= 1.0;
}

}  // namespace

TEST(RandomizedConfidenceUtilsTest, GenerateRandomizedConfidenceLevelNoNoise) {
  blink::mojom::ConfidenceLevel input_values[] = {
      blink::mojom::ConfidenceLevel::kHigh,
      blink::mojom::ConfidenceLevel::kLow};

  for (const auto& input_value : input_values) {
    for (int i = 0; i < 1000; i++) {
      blink::mojom::ConfidenceLevel randomized_confidence =
          GenerateRandomizedConfidenceLevel(0.0, input_value);
      EXPECT_EQ(randomized_confidence, input_value);
    }
  }
}

TEST(RandomizedConfidenceUtilsTest,
     GenerateRandomizedConfidenceLevelDistribution) {
  blink::mojom::ConfidenceLevel input_values[] = {
      blink::mojom::ConfidenceLevel::kHigh,
      blink::mojom::ConfidenceLevel::kLow};

  for (const auto& input_value : input_values) {
    int match_count = 0;

    for (int i = 0; i < 1000; i++) {
      blink::mojom::ConfidenceLevel randomized_confidence =
          GenerateRandomizedConfidenceLevel(0.5, input_value);
      ASSERT_PRED1(IsComputedConfidenceInExpectedRange, randomized_confidence);
      if (randomized_confidence == input_value) {
        match_count++;
      }
    }

    // With an epsilon value of ~1.1, we expect input_value
    // to be returned ~75% of the time. Validate that this is
    // the case allowing for some noise.
    LOG(INFO) << input_value << " :" << match_count;
    EXPECT_NEAR(750, match_count, 50);
  }
}

TEST(RandomizedConfidenceUtilsTest, GetConfidenceRandomizedTriggerRate) {
  double trigger_rate = GetConfidenceRandomizedTriggerRate();
  EXPECT_PRED1(IsRandomizedTriggerRateInExpectedRange, trigger_rate);
}

TEST(RandomizedConfidenceUtilsTest,
     GetConfidenceRandomizedTriggerRateAllNoise) {
  base::test::ScopedFeatureList scoped_feature_list;

  // Set the epsilon value to 0.0 to ensure we always get a randomly
  // generated value back.
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kNavigationConfidenceEpsilon,
                             {{"navigation-confidence-epsilon-value", "0.0"}}}},
      /*disabled_features=*/{});

  double trigger_rate = GetConfidenceRandomizedTriggerRate();
  EXPECT_DOUBLE_EQ(1.0, trigger_rate);
}

TEST(RandomizedConfidenceUtilsTest, GetConfidenceRandomizedTriggerRateNoNoise) {
  base::test::ScopedFeatureList scoped_feature_list;

  // Set the epsilon value to 0.0 to ensure we always get a randomly
  // generated value back.
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kNavigationConfidenceEpsilon,
                             {{"navigation-confidence-epsilon-value",
                               "18.0"}}}},
      /*disabled_features=*/{});

  double trigger_rate = GetConfidenceRandomizedTriggerRate();
  EXPECT_DOUBLE_EQ(0.0, trigger_rate);
}

}  // namespace content
