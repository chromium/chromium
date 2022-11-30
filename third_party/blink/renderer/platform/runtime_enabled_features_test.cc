// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class RuntimeEnabledFeaturesTest : public testing::Test {
  void CheckAllDisabled() {
    CHECK(!RuntimeEnabledFeatures::TestFeatureEnabled());
    CHECK(!RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
    CHECK(!RuntimeEnabledFeatures::TestFeatureDependentEnabled());
    CHECK(!RuntimeEnabledFeatures::OriginTrialsSampleAPIEnabledByRuntimeFlag());
    CHECK(!RuntimeEnabledFeatures::
              OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
    CHECK(!RuntimeEnabledFeatures::
              OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());
  }
  void SetUp() override { CheckAllDisabled(); }
  void TearDown() override {
    backup_.Restore();
    CheckAllDisabled();
  }
  RuntimeEnabledFeatures::Backup backup_;
};

// Test setup:
//   TestFeatureDependent
// depends_on
//   TestFeatureImplied
// implied_by
//   TestFeature

TEST_F(RuntimeEnabledFeaturesTest, Relationship) {
  // Internal status: false, false, false.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::SetTestFeatureEnabled(true);
  // Internal status: true, false, false.
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::SetTestFeatureImpliedEnabled(true);
  // Internal status: true, true, false.
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::SetTestFeatureDependentEnabled(true);
  // Internal status: true, true, true.
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::SetTestFeatureImpliedEnabled(false);
  // Internal status: true, false, true.
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::SetTestFeatureEnabled(false);
  // Internal status: false, false, true.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  // Depends on TestFeatureImplied.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::SetTestFeatureImpliedEnabled(true);
  // Internal status: false, true, true.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::SetTestFeatureDependentEnabled(false);
  // Internal status: false, true, false.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
}

TEST_F(RuntimeEnabledFeaturesTest, ScopedForTest) {
  // Internal status: false, false, false.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
  {
    ScopedTestFeatureForTest f1(true);
    // Internal status: true, false, false.
    EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
    // Implied by TestFeature.
    EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
    EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
    {
      ScopedTestFeatureImpliedForTest f2(true);
      // Internal status: true, true, false.
      EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
      EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
      EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
      {
        ScopedTestFeatureDependentForTest f3(true);
        // Internal status: true, true, true.
        EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
        EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
        EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
        {
          ScopedTestFeatureDependentForTest f3a(false);
          // Internal status: true, true, true.
          EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
          EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
          EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
        }
        // Internal status: true, true, true.
        EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
        EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
        EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
      }
    }
    // Internal status: true, false, false.
    EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
    // Implied by TestFeature.
    EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
    EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
    {
      ScopedTestFeatureImpliedForTest f2a(false);
      // Internal status: true, false, false.
      EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
      // Implied by TestFeature.
      EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
      EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
    }
  }
  // Internal status: false, false, false.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  {
    ScopedTestFeatureDependentForTest f3(true);
    // Internal status: false, false, true.
    EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
    EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
    // Depends on TestFeatureImplied.
    EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
    {
      ScopedTestFeatureImpliedForTest f2(true);
      // Internal status: false, true, true.
      EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
      EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
      EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
      {
        ScopedTestFeatureForTest f1(true);
        // Internal status: true, true, true.
        EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
        EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
        EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
      }
      // Internal status: false, true, true.
      EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
      EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
      EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
    }
    // Internal status: false, false, true.
    EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
    EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
    // Depends on TestFeatureImplied.
    EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
    {
      ScopedTestFeatureImpliedForTest f2(true);
      // Internal status: false, true, true.
      EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
      EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
      EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
    }
  }
  // Internal status: false, false, false.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
}

TEST_F(RuntimeEnabledFeaturesTest, BackupRestore) {
  // Internal status: false, false, false.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::SetTestFeatureEnabled(true);
  RuntimeEnabledFeatures::SetTestFeatureDependentEnabled(true);
  // Internal status: true, false, true.
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::Backup backup;

  RuntimeEnabledFeatures::SetTestFeatureEnabled(false);
  RuntimeEnabledFeatures::SetTestFeatureImpliedEnabled(true);
  RuntimeEnabledFeatures::SetTestFeatureDependentEnabled(false);
  // Internal status: false, true, false.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  backup.Restore();
  // Should restore the internal status to: true, false, true.
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::SetTestFeatureEnabled(false);
  // Internal status: false, false, true.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureImpliedEnabled());
  // Depends on TestFeatureImplied.
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureDependentEnabled());
}

// Test setup:
// OriginTrialsSampleAPIImplied   impled_by  \
//                                             OriginTrialsSampleAPI
// OriginTrialsSampleAPIDependent depends_on /
TEST_F(RuntimeEnabledFeaturesTest, OriginTrialsByRuntimeEnabled) {
  // Internal status: false, false, false.
  EXPECT_FALSE(
      RuntimeEnabledFeatures::OriginTrialsSampleAPIEnabledByRuntimeFlag());
  EXPECT_FALSE(RuntimeEnabledFeatures::
                   OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  EXPECT_FALSE(RuntimeEnabledFeatures::
                   OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());

  RuntimeEnabledFeatures::SetOriginTrialsSampleAPIEnabled(true);
  // Internal status: true, false, false.
  EXPECT_TRUE(
      RuntimeEnabledFeatures::OriginTrialsSampleAPIEnabledByRuntimeFlag());
  // Implied by OriginTrialsSampleAPI.
  EXPECT_TRUE(RuntimeEnabledFeatures::
                  OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  EXPECT_FALSE(RuntimeEnabledFeatures::
                   OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());

  RuntimeEnabledFeatures::SetOriginTrialsSampleAPIImpliedEnabled(true);
  RuntimeEnabledFeatures::SetOriginTrialsSampleAPIDependentEnabled(true);
  // Internal status: true, true, true.
  EXPECT_TRUE(
      RuntimeEnabledFeatures::OriginTrialsSampleAPIEnabledByRuntimeFlag());
  EXPECT_TRUE(RuntimeEnabledFeatures::
                  OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  EXPECT_TRUE(RuntimeEnabledFeatures::
                  OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());

  RuntimeEnabledFeatures::SetOriginTrialsSampleAPIEnabled(false);
  // Internal status: false, true, true.
  EXPECT_FALSE(
      RuntimeEnabledFeatures::OriginTrialsSampleAPIEnabledByRuntimeFlag());
  EXPECT_TRUE(RuntimeEnabledFeatures::
                  OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  // Depends on OriginTrialsSampleAPI.
  EXPECT_FALSE(RuntimeEnabledFeatures::
                   OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());
}

}  // namespace blink
