// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

#include <cstdlib>

#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/platform/runtime_enabled_feature_checks.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

// The setters are protected, and RuntimeEnabledFeatures only befriends named
// classes, so the tests reach them through this fixture.
class RuntimeEnabledFeaturesTest : public testing::Test {
 protected:
  void CheckAllDisabled() {
    CHECK(!TestFeatureEnabled());
    CHECK(!TestFeatureImpliedEnabled());
    CHECK(!TestFeatureDependentEnabled());
    CHECK(!OriginTrialsSampleAPIEnabledByRuntimeFlag());
    CHECK(!OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
    CHECK(!OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());
  }
  void SetUp() override { CheckAllDisabled(); }
  void TearDown() override {
    backup_.Restore();
    CheckAllDisabled();
  }

  bool TestFeatureEnabled() {
    return RuntimeEnabledFeatures::TestFeatureEnabled();
  }

  bool TestFeatureImpliedEnabled() {
    return RuntimeEnabledFeatures::TestFeatureImpliedEnabled();
  }

  bool TestFeatureDependentEnabled() {
    return RuntimeEnabledFeatures::TestFeatureDependentEnabled();
  }

  bool OriginTrialsSampleAPIEnabledByRuntimeFlag() {
    return RuntimeEnabledFeatures::OriginTrialsSampleAPIEnabledByRuntimeFlag();
  }

  bool OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag() {
    return RuntimeEnabledFeatures::
        OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag();
  }

  bool OriginTrialsSampleAPIDependentEnabledByRuntimeFlag() {
    return RuntimeEnabledFeatures::
        OriginTrialsSampleAPIDependentEnabledByRuntimeFlag();
  }

  void SetTestFeatureEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetTestFeatureEnabled(enabled);
  }

  void SetTestFeatureImpliedEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetTestFeatureImpliedEnabled(enabled);
  }

  void SetTestFeatureDependentEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetTestFeatureDependentEnabled(enabled);
  }

  void SetOriginTrialsSampleAPIEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetOriginTrialsSampleAPIEnabled(enabled);
  }

  void SetOriginTrialsSampleAPIImpliedEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetOriginTrialsSampleAPIImpliedEnabled(enabled);
  }

  void SetOriginTrialsSampleAPIDependentEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetOriginTrialsSampleAPIDependentEnabled(enabled);
  }

 private:
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
  EXPECT_FALSE(TestFeatureEnabled());
  EXPECT_FALSE(TestFeatureImpliedEnabled());
  EXPECT_FALSE(TestFeatureDependentEnabled());

  SetTestFeatureEnabled(true);
  // Internal status: true, false, false.
  EXPECT_TRUE(TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(TestFeatureImpliedEnabled());
  EXPECT_FALSE(TestFeatureDependentEnabled());

  SetTestFeatureImpliedEnabled(true);
  // Internal status: true, true, false.
  EXPECT_TRUE(TestFeatureEnabled());
  EXPECT_TRUE(TestFeatureImpliedEnabled());
  EXPECT_FALSE(TestFeatureDependentEnabled());

  SetTestFeatureDependentEnabled(true);
  // Internal status: true, true, true.
  EXPECT_TRUE(TestFeatureEnabled());
  EXPECT_TRUE(TestFeatureImpliedEnabled());
  EXPECT_TRUE(TestFeatureDependentEnabled());

  SetTestFeatureImpliedEnabled(false);
  // Internal status: true, false, true.
  EXPECT_TRUE(TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(TestFeatureImpliedEnabled());
  EXPECT_TRUE(TestFeatureDependentEnabled());

  SetTestFeatureEnabled(false);
  // Internal status: false, false, true.
  EXPECT_FALSE(TestFeatureEnabled());
  EXPECT_FALSE(TestFeatureImpliedEnabled());
  // Depends on TestFeatureImplied.
  EXPECT_FALSE(TestFeatureDependentEnabled());

  SetTestFeatureImpliedEnabled(true);
  // Internal status: false, true, true.
  EXPECT_FALSE(TestFeatureEnabled());
  EXPECT_TRUE(TestFeatureImpliedEnabled());
  EXPECT_TRUE(TestFeatureDependentEnabled());

  SetTestFeatureDependentEnabled(false);
  // Internal status: false, true, false.
  EXPECT_FALSE(TestFeatureEnabled());
  EXPECT_TRUE(TestFeatureImpliedEnabled());
  EXPECT_FALSE(TestFeatureDependentEnabled());
}

TEST_F(RuntimeEnabledFeaturesTest, ScopedForTest) {
  // Internal status: false, false, false.
  EXPECT_FALSE(TestFeatureEnabled());
  EXPECT_FALSE(TestFeatureImpliedEnabled());
  EXPECT_FALSE(TestFeatureDependentEnabled());
  {
    ScopedTestFeatureForTest f1(true);
    // Internal status: true, false, false.
    EXPECT_TRUE(TestFeatureEnabled());
    // Implied by TestFeature.
    EXPECT_TRUE(TestFeatureImpliedEnabled());
    EXPECT_FALSE(TestFeatureDependentEnabled());
    {
      ScopedTestFeatureImpliedForTest f2(true);
      // Internal status: true, true, false.
      EXPECT_TRUE(TestFeatureEnabled());
      EXPECT_TRUE(TestFeatureImpliedEnabled());
      EXPECT_FALSE(TestFeatureDependentEnabled());
      {
        ScopedTestFeatureDependentForTest f3(true);
        // Internal status: true, true, true.
        EXPECT_TRUE(TestFeatureEnabled());
        EXPECT_TRUE(TestFeatureImpliedEnabled());
        EXPECT_TRUE(TestFeatureDependentEnabled());
        {
          ScopedTestFeatureDependentForTest f3a(false);
          // Internal status: true, true, true.
          EXPECT_TRUE(TestFeatureEnabled());
          EXPECT_TRUE(TestFeatureImpliedEnabled());
          EXPECT_FALSE(TestFeatureDependentEnabled());
        }
        // Internal status: true, true, true.
        EXPECT_TRUE(TestFeatureEnabled());
        EXPECT_TRUE(TestFeatureImpliedEnabled());
        EXPECT_TRUE(TestFeatureDependentEnabled());
      }
    }
    // Internal status: true, false, false.
    EXPECT_TRUE(TestFeatureEnabled());
    // Implied by TestFeature.
    EXPECT_TRUE(TestFeatureImpliedEnabled());
    EXPECT_FALSE(TestFeatureDependentEnabled());
    {
      ScopedTestFeatureImpliedForTest f2a(false);
      // Internal status: true, false, false.
      EXPECT_TRUE(TestFeatureEnabled());
      // Implied by TestFeature.
      EXPECT_TRUE(TestFeatureImpliedEnabled());
      EXPECT_FALSE(TestFeatureDependentEnabled());
    }
  }
  // Internal status: false, false, false.
  EXPECT_FALSE(TestFeatureEnabled());
  EXPECT_FALSE(TestFeatureImpliedEnabled());
  EXPECT_FALSE(TestFeatureDependentEnabled());
  {
    ScopedTestFeatureDependentForTest f3(true);
    // Internal status: false, false, true.
    EXPECT_FALSE(TestFeatureEnabled());
    EXPECT_FALSE(TestFeatureImpliedEnabled());
    // Depends on TestFeatureImplied.
    EXPECT_FALSE(TestFeatureDependentEnabled());
    {
      ScopedTestFeatureImpliedForTest f2(true);
      // Internal status: false, true, true.
      EXPECT_FALSE(TestFeatureEnabled());
      EXPECT_TRUE(TestFeatureImpliedEnabled());
      EXPECT_TRUE(TestFeatureDependentEnabled());
      {
        ScopedTestFeatureForTest f1(true);
        // Internal status: true, true, true.
        EXPECT_TRUE(TestFeatureEnabled());
        EXPECT_TRUE(TestFeatureImpliedEnabled());
        EXPECT_TRUE(TestFeatureDependentEnabled());
      }
      // Internal status: false, true, true.
      EXPECT_FALSE(TestFeatureEnabled());
      EXPECT_TRUE(TestFeatureImpliedEnabled());
      EXPECT_TRUE(TestFeatureDependentEnabled());
    }
    // Internal status: false, false, true.
    EXPECT_FALSE(TestFeatureEnabled());
    EXPECT_FALSE(TestFeatureImpliedEnabled());
    // Depends on TestFeatureImplied.
    EXPECT_FALSE(TestFeatureDependentEnabled());
    {
      ScopedTestFeatureImpliedForTest f2(true);
      // Internal status: false, true, true.
      EXPECT_FALSE(TestFeatureEnabled());
      EXPECT_TRUE(TestFeatureImpliedEnabled());
      EXPECT_TRUE(TestFeatureDependentEnabled());
    }
  }
  // Internal status: false, false, false.
  EXPECT_FALSE(TestFeatureEnabled());
  EXPECT_FALSE(TestFeatureImpliedEnabled());
  EXPECT_FALSE(TestFeatureDependentEnabled());
}

TEST_F(RuntimeEnabledFeaturesTest, BackupRestore) {
  // Internal status: false, false, false.
  EXPECT_FALSE(TestFeatureEnabled());
  EXPECT_FALSE(TestFeatureImpliedEnabled());
  EXPECT_FALSE(TestFeatureDependentEnabled());

  SetTestFeatureEnabled(true);
  SetTestFeatureDependentEnabled(true);
  // Internal status: true, false, true.
  EXPECT_TRUE(TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(TestFeatureImpliedEnabled());
  EXPECT_TRUE(TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::Backup backup;

  SetTestFeatureEnabled(false);
  SetTestFeatureImpliedEnabled(true);
  SetTestFeatureDependentEnabled(false);
  // Internal status: false, true, false.
  EXPECT_FALSE(TestFeatureEnabled());
  EXPECT_TRUE(TestFeatureImpliedEnabled());
  EXPECT_FALSE(TestFeatureDependentEnabled());

  backup.Restore();
  // Should restore the internal status to: true, false, true.
  EXPECT_TRUE(TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(TestFeatureImpliedEnabled());
  EXPECT_TRUE(TestFeatureDependentEnabled());

  SetTestFeatureEnabled(false);
  // Internal status: false, false, true.
  EXPECT_FALSE(TestFeatureEnabled());
  EXPECT_FALSE(TestFeatureImpliedEnabled());
  // Depends on TestFeatureImplied.
  EXPECT_FALSE(TestFeatureDependentEnabled());
}

// Test setup:
// OriginTrialsSampleAPIImplied   impled_by  \
//                                             OriginTrialsSampleAPI
// OriginTrialsSampleAPIDependent depends_on /
TEST_F(RuntimeEnabledFeaturesTest, OriginTrialsByRuntimeEnabled) {
  // Internal status: false, false, false.
  EXPECT_FALSE(OriginTrialsSampleAPIEnabledByRuntimeFlag());
  EXPECT_FALSE(OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  EXPECT_FALSE(OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());

  SetOriginTrialsSampleAPIEnabled(true);
  // Internal status: true, false, false.
  EXPECT_TRUE(OriginTrialsSampleAPIEnabledByRuntimeFlag());
  // Implied by OriginTrialsSampleAPI.
  EXPECT_TRUE(OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  EXPECT_FALSE(OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());

  SetOriginTrialsSampleAPIImpliedEnabled(true);
  SetOriginTrialsSampleAPIDependentEnabled(true);
  // Internal status: true, true, true.
  EXPECT_TRUE(OriginTrialsSampleAPIEnabledByRuntimeFlag());
  EXPECT_TRUE(OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  EXPECT_TRUE(OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());

  SetOriginTrialsSampleAPIEnabled(false);
  // Internal status: false, true, true.
  EXPECT_FALSE(OriginTrialsSampleAPIEnabledByRuntimeFlag());
  EXPECT_TRUE(OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  // Depends on OriginTrialsSampleAPI.
  EXPECT_FALSE(OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());
}

TEST_F(RuntimeEnabledFeaturesTest, CopiedFromBaseFaetureIf) {
  using base::FeatureList;
  const base::Feature& kFeature = features::kTestBlinkFeatureDefault;
  ASSERT_TRUE(FeatureList::IsEnabled(kFeature));
  ASSERT_TRUE(FeatureList::GetInstance()->IsFeatureOverridden(kFeature.name));
  ASSERT_FALSE(FeatureList::GetStateIfOverridden(kFeature));
  WebRuntimeFeatures::UpdateStatusFromBaseFeatures();
  EXPECT_FALSE(RuntimeEnabledFeatures::TestBlinkFeatureDefaultEnabled());
}

TEST_F(RuntimeEnabledFeaturesTest, FocusgroupV2CanBeToggled) {
  ScopedFocusgroupV2ForTest v2_disabled(false);
  EXPECT_FALSE(RuntimeEnabledFeatures::FocusgroupV2Enabled());

  {
    ScopedFocusgroupV2ForTest v2_enabled(true);
    EXPECT_TRUE(RuntimeEnabledFeatures::FocusgroupV2Enabled());
  }

  EXPECT_FALSE(RuntimeEnabledFeatures::FocusgroupV2Enabled());
}

TEST_F(RuntimeEnabledFeaturesTest, FocusgroupV2DependsOnFocusgroup) {
  ScopedFocusgroupForTest focusgroup_disabled(false);
  ScopedFocusgroupV2ForTest v2_disabled(false);
  EXPECT_FALSE(RuntimeEnabledFeatures::FocusgroupEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::FocusgroupV2Enabled());

  {
    ScopedFocusgroupV2ForTest v2_enabled(true);
    EXPECT_FALSE(RuntimeEnabledFeatures::FocusgroupV2Enabled());
  }

  {
    ScopedFocusgroupForTest focusgroup_enabled(true);
    ScopedFocusgroupV2ForTest v2_enabled(true);
    EXPECT_TRUE(RuntimeEnabledFeatures::FocusgroupEnabled());
    EXPECT_TRUE(RuntimeEnabledFeatures::FocusgroupV2Enabled());
  }

  EXPECT_FALSE(RuntimeEnabledFeatures::FocusgroupEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::FocusgroupV2Enabled());
}

TEST_F(RuntimeEnabledFeaturesTest, CustomEnableCheckAllowed) {
  ScopedTestFeatureAllowedForTest allowed(true);

  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureCustomEnableCheckEnabled());
  {
    ScopedTestFeatureCustomEnableCheckForTest enabled(true);
    EXPECT_TRUE(RuntimeEnabledFeatures::TestFeatureCustomEnableCheckEnabled());
    EXPECT_TRUE(RuntimeEnabledFeatures::IsFeatureEnabledFromString(
        "TestFeatureCustomEnableCheck"));
  }
  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureCustomEnableCheckEnabled());
}

TEST_F(RuntimeEnabledFeaturesTest, CustomEnableCheckDisallowed) {
  ScopedTestFeatureAllowedForTest disallowed(false);

  EXPECT_FALSE(RuntimeEnabledFeatures::TestFeatureCustomEnableCheckEnabled());
  EXPECT_FALSE(RuntimeEnabledFeatures::IsFeatureEnabledFromString(
      "TestFeatureCustomEnableCheck"));
}

#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
TEST(RuntimeEnabledFeaturesCustomEnableCheckDeathTest, SetButNotAllowed) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  ScopedTestFeatureCustomEnableCheckForTest enabled(true);
  ScopedTestFeatureAllowedForTest disallowed(false);

  EXPECT_CHECK_DEATH(
      RuntimeEnabledFeatures::TestFeatureCustomEnableCheckEnabled());
  EXPECT_CHECK_DEATH(RuntimeEnabledFeatures::IsFeatureEnabledFromString(
      "TestFeatureCustomEnableCheck"));
}
#endif  // defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

// blink_platform_unittests enables test-only features, which grants all of the
// MojoJS permissions, so both features should be enabled and readable here.
TEST_F(RuntimeEnabledFeaturesTest, MojoJSAllowedByTestOnlyFeatures) {
  EXPECT_TRUE(RuntimeEnabledFeatures::MojoJSEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::MojoJSTestEnabled());
}

// With the permission granted, the MojoJS features behave like any other
// runtime feature.
TEST_F(RuntimeEnabledFeaturesTest, MojoJSCanBeToggledWhenAllowed) {
  ASSERT_TRUE(IsMojoJSAllowedForProcess());

  ScopedMojoJSForTest disabled(false);
  EXPECT_FALSE(RuntimeEnabledFeatures::MojoJSEnabled());
  {
    ScopedMojoJSForTest enabled(true);
    EXPECT_TRUE(RuntimeEnabledFeatures::MojoJSEnabled());
  }
  EXPECT_FALSE(RuntimeEnabledFeatures::MojoJSEnabled());
}

#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
TEST(RuntimeEnabledFeaturesMojoJSDeathTest, SetButNotAllowed) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  EXPECT_CHECK_DEATH({
    // Enabling the flag in `RuntimeEnabledFeatures` in a disallowed process
    // should crash with a CHECK failure.
    ScopedDisallowMojoJsForTesting disallowed;
    ScopedMojoJSForTest enabled(true);
    (void)RuntimeEnabledFeatures::MojoJSEnabled();
  });
}
#endif  // defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

}  // namespace blink
