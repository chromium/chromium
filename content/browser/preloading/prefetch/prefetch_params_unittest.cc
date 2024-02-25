// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_params.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class PrefetchParamsTest : public ::testing::Test {};

TEST_F(PrefetchParamsTest, DecoyProbabilityClampedZero) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPrefetchUseContentRefactor,
      {{"ineligible_decoy_request_probability", "-1"}});

  for (size_t i = 0; i < 100; i++) {
    EXPECT_FALSE(PrefetchServiceSendDecoyRequestForIneligblePrefetch(
        /* disabled_based_on_user_settings=*/false));
  }
}

TEST_F(PrefetchParamsTest, DecoyProbabilityClampedOne) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPrefetchUseContentRefactor,
      {{"ineligible_decoy_request_probability", "2"}});

  for (size_t i = 0; i < 100; i++) {
    EXPECT_TRUE(PrefetchServiceSendDecoyRequestForIneligblePrefetch(
        /* disabled_based_on_user_settings=*/false));
  }
}

}  // namespace
}  // namespace content
