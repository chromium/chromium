// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

TEST(ResourceBrokerFeaturesTest, DefaultState) {
  EXPECT_FALSE(base::FeatureList::IsEnabled(features::kResourceBroker));
}

TEST(ResourceBrokerFeaturesTest, DefaultParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kResourceBroker);

  // Asserting default param values.
  // Note: params on a disabled feature return defaults trivially, which tests
  // nothing, so we explicitly enable the feature first.
  EXPECT_EQ(features::kResourceBrokerGraceWindow.Get(), base::Seconds(300));
}

TEST(ResourceBrokerFeaturesTest, OverriddenParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kResourceBroker,
      {{features::kResourceBrokerGraceWindow.name, "600s"}});

  EXPECT_EQ(features::kResourceBrokerGraceWindow.Get(), base::Seconds(600));
}

TEST(ResourceBrokerFeaturesTest, InvalidParamFallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kResourceBroker,
      {{features::kResourceBrokerGraceWindow.name, "not_a_time"}});

  EXPECT_EQ(features::kResourceBrokerGraceWindow.Get(), base::Seconds(300));
}

}  // namespace

}  // namespace content
