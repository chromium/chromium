// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_config.h"

#include "base/test/scoped_feature_list.h"
#include "content/common/features.h"
#include "content/public/browser/preloading.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class PreloadingConfigTest : public ::testing::Test {};

TEST_F(PreloadingConfigTest, EmptyConfig) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kPreloadingConfig,
                                              {{"preloading_config", ""}});
  PreloadingConfig& config = PreloadingConfig::GetInstance();
  config.ParseConfig();
  // Unspecified configs should not be held back.
  EXPECT_FALSE(
      config.ShouldHoldback(PreloadingType::kPreconnect,
                            preloading_predictor::kUrlPointerDownOnAnchor));
  // Unspecified configs should log 100% of PreloadingAttempts.
  EXPECT_EQ(
      config.SamplingLikelihood(PreloadingType::kPreconnect,
                                preloading_predictor::kUrlPointerDownOnAnchor),
      1.0);
}

TEST_F(PreloadingConfigTest, NonJsonConfig) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kPreloadingConfig,
                                              {{"preloading_config", "bad"}});
  PreloadingConfig& config = PreloadingConfig::GetInstance();
  config.ParseConfig();
  // Unspecified configs should not be held back.
  EXPECT_FALSE(
      config.ShouldHoldback(PreloadingType::kPreconnect,
                            preloading_predictor::kUrlPointerDownOnAnchor));
  // Unspecified configs should log 100% of PreloadingAttempts.
  EXPECT_EQ(
      config.SamplingLikelihood(PreloadingType::kPreconnect,
                                preloading_predictor::kUrlPointerDownOnAnchor),
      1.0);
}

TEST_F(PreloadingConfigTest, InvalidPreloadingType) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kPreloadingConfig,
                                              {{"preloading_config", R"(
  [{
    "preloading_type": "Foo",
    "preloading_predictor": "UrlPointerDownOnAnchor",
    "holdback": true
  }]
  )"}});
  PreloadingConfig& config = PreloadingConfig::GetInstance();
  config.ParseConfig();
  // Unspecified configs should not be held back.
  EXPECT_FALSE(
      config.ShouldHoldback(PreloadingType::kPreconnect,
                            preloading_predictor::kUrlPointerDownOnAnchor));
}

TEST_F(PreloadingConfigTest, InvalidPredictor) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kPreloadingConfig,
                                              {{"preloading_config", R"(
  [{
    "preloading_type": "Preconnect",
    "preloading_predictor": "Foo",
    "holdback": true
  }]
  )"}});
  PreloadingConfig& config = PreloadingConfig::GetInstance();
  config.ParseConfig();
  // Unspecified configs should not be held back.
  EXPECT_FALSE(
      config.ShouldHoldback(PreloadingType::kPreconnect,
                            preloading_predictor::kUrlPointerDownOnAnchor));
}

TEST_F(PreloadingConfigTest, ValidConfig) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kPreloadingConfig,
                                              {{"preloading_config", R"(
  [{
    "preloading_type": "Preconnect",
    "preloading_predictor": "UrlPointerDownOnAnchor",
    "holdback": true,
    "sampling_likelihood": 0.5
  },{
    "preloading_type": "Prerender",
    "preloading_predictor": "UrlPointerHoverOnAnchor",
    "holdback": true,
    "sampling_likelihood": 0.25
  }]
  )"}});
  PreloadingConfig& config = PreloadingConfig::GetInstance();
  config.ParseConfig();
  EXPECT_TRUE(
      config.ShouldHoldback(PreloadingType::kPreconnect,
                            preloading_predictor::kUrlPointerDownOnAnchor));
  EXPECT_EQ(
      config.SamplingLikelihood(PreloadingType::kPreconnect,
                                preloading_predictor::kUrlPointerDownOnAnchor),
      0.5);
  EXPECT_TRUE(
      config.ShouldHoldback(PreloadingType::kPrerender,
                            preloading_predictor::kUrlPointerHoverOnAnchor));
  EXPECT_EQ(
      config.SamplingLikelihood(PreloadingType::kPrerender,
                                preloading_predictor::kUrlPointerHoverOnAnchor),
      0.25);
  // This isn't in the config, so make sure we get the default settings.
  EXPECT_FALSE(
      config.ShouldHoldback(PreloadingType::kPrerender,
                            preloading_predictor::kUrlPointerDownOnAnchor));
  EXPECT_EQ(
      config.SamplingLikelihood(PreloadingType::kPrerender,
                                preloading_predictor::kUrlPointerDownOnAnchor),
      1.0);
}

}  // namespace
}  // namespace content
