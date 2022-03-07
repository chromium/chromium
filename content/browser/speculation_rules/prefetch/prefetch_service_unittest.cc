// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_service.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/speculation_rules/prefetch/prefetch_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class PrefetchServiceTest : public ::testing::Test {};

TEST_F(PrefetchServiceTest, CreateServiceWhenFeatureEnabled) {
  // Enable feature, which means that we should be able to create a
  // PrefetchService instance.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      content::features::kPrefetchUseContentRefactor);

  EXPECT_TRUE(PrefetchService::CreateIfPossible());
}

TEST_F(PrefetchServiceTest, DontCreateServiceWhenFeatureDisabled) {
  // Disable feature, which means that we shouldn't be able to create a
  // PrefetchService instance.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      content::features::kPrefetchUseContentRefactor);

  EXPECT_FALSE(PrefetchService::CreateIfPossible());
}

}  // namespace
}  // namespace content
