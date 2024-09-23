// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class RuntimeEnabledFeaturesTestTraits {
 public:
  using ScopedTestFeatureForTestType = ScopedTestFeatureForTest;
  using ScopedTestFeatureImpliedForTestType = ScopedTestFeatureImpliedForTest;
  using ScopedTestFeatureDependentForTestType =
      ScopedTestFeatureDependentForTest;

  static bool ScopedForTestSupported() { return true; }

  static ScopedTestFeatureForTestType CreateScopedTestFeatureForTest(
      bool enabled) {
    return ScopedTestFeatureForTest(enabled);
  }

  static ScopedTestFeatureImpliedForTestType
  CreateScopedTestFeatureImpliedForTest(bool enabled) {
    return ScopedTestFeatureImpliedForTest(enabled);
  }

  static ScopedTestFeatureDependentForTestType
  CreateScopedTestFeatureDependentForTest(bool enabled) {
    return ScopedTestFeatureDependentForTest(enabled);
  }

  static bool TestFeatureEnabled() {
    return RuntimeEnabledFeatures::TestFeatureEnabled();
  }

  static bool TestFeatureImpliedEnabled() {
    return RuntimeEnabledFeatures::TestFeatureImpliedEnabled();
  }

  static bool TestFeatureDependentEnabled() {
    return RuntimeEnabledFeatures::TestFeatureDependentEnabled();
  }

  static bool OriginTrialsSampleAPIEnabledByRuntimeFlag() {
    return RuntimeEnabledFeatures::OriginTrialsSampleAPIEnabledByRuntimeFlag();
  }

  static bool OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag() {
    return RuntimeEnabledFeatures::
        OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag();
  }

  static bool OriginTrialsSampleAPIDependentEnabledByRuntimeFlag() {
    return RuntimeEnabledFeatures::
        OriginTrialsSampleAPIDependentEnabledByRuntimeFlag();
  }

  static void SetTestFeatureEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetTestFeatureEnabled(enabled);
  }

  static void SetTestFeatureImpliedEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetTestFeatureImpliedEnabled(enabled);
  }

  static void SetTestFeatureDependentEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetTestFeatureDependentEnabled(enabled);
  }

  static void SetOriginTrialsSampleAPIEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetOriginTrialsSampleAPIEnabled(enabled);
  }

  static void SetOriginTrialsSampleAPIImpliedEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetOriginTrialsSampleAPIImpliedEnabled(enabled);
  }

  static void SetOriginTrialsSampleAPIDependentEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetOriginTrialsSampleAPIDependentEnabled(enabled);
  }
};

class RuntimeProtectedEnabledFeaturesTestTraits {
 public:
  using ScopedTestFeatureForTestType = ScopedTestFeatureProtectedForTest;
  using ScopedTestFeatureImpliedForTestType =
      ScopedTestFeatureProtectedImpliedForTest;
  using ScopedTestFeatureDependentForTestType =
      ScopedTestFeatureProtectedDependentForTest;

  static bool ScopedForTestSupported() {
    // The way the ScopedForTest classes are implemented, they do not work with
    // protected variables in component builds. This is because of the static
    // inline variable use results in the value being allocated in one module,
    // but the protected code being called from a different. So don't run this
    // test for the protected case in component builds.
#if defined(COMPONENT_BUILD)
    return false;
#else
    return true;
#endif
  }

  static ScopedTestFeatureForTestType CreateScopedTestFeatureForTest(
      bool enabled) {
    return ScopedTestFeatureProtectedForTest(enabled);
  }

  static ScopedTestFeatureImpliedForTestType
  CreateScopedTestFeatureImpliedForTest(bool enabled) {
    return ScopedTestFeatureProtectedImpliedForTest(enabled);
  }

  static ScopedTestFeatureDependentForTestType
  CreateScopedTestFeatureDependentForTest(bool enabled) {
    return ScopedTestFeatureProtectedDependentForTest(enabled);
  }

  static bool TestFeatureEnabled() {
    return RuntimeEnabledFeatures::TestFeatureProtectedEnabled();
  }

  static bool TestFeatureImpliedEnabled() {
    return RuntimeEnabledFeatures::TestFeatureProtectedImpliedEnabled();
  }

  static bool TestFeatureDependentEnabled() {
    return RuntimeEnabledFeatures::TestFeatureProtectedDependentEnabled();
  }

  static bool OriginTrialsSampleAPIEnabledByRuntimeFlag() {
    return RuntimeEnabledFeatures::
        ProtectedOriginTrialsSampleAPIEnabledByRuntimeFlag();
  }

  static bool OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag() {
    return RuntimeEnabledFeatures::
        ProtectedOriginTrialsSampleAPIImpliedEnabledByRuntimeFlag();
  }

  static bool OriginTrialsSampleAPIDependentEnabledByRuntimeFlag() {
    return RuntimeEnabledFeatures::
        ProtectedOriginTrialsSampleAPIDependentEnabledByRuntimeFlag();
  }

  static void SetTestFeatureEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetTestFeatureProtectedEnabled(enabled);
  }

  static void SetTestFeatureImpliedEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetTestFeatureProtectedImpliedEnabled(enabled);
  }

  static void SetTestFeatureDependentEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetTestFeatureProtectedDependentEnabled(enabled);
  }

  static void SetOriginTrialsSampleAPIEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetProtectedOriginTrialsSampleAPIEnabled(enabled);
  }

  static void SetOriginTrialsSampleAPIImpliedEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetProtectedOriginTrialsSampleAPIImpliedEnabled(
        enabled);
  }

  static void SetOriginTrialsSampleAPIDependentEnabled(bool enabled) {
    RuntimeEnabledFeatures::SetProtectedOriginTrialsSampleAPIDependentEnabled(
        enabled);
  }
};

template <typename TRuntimeEnabledFeaturesTraits>
class AbstractRuntimeEnabledFeaturesTest : public testing::Test {
 protected:
  using ScopedTestFeatureForTestType =
      typename TRuntimeEnabledFeaturesTraits::ScopedTestFeatureForTestType;
  using ScopedTestFeatureImpliedForTestType =
      typename TRuntimeEnabledFeaturesTraits::
          ScopedTestFeatureImpliedForTestType;
  using ScopedTestFeatureDependentForTestType =
      typename TRuntimeEnabledFeaturesTraits::
          ScopedTestFeatureDependentForTestType;

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

  bool ScopedForTestSupported() {
    return TRuntimeEnabledFeaturesTraits::ScopedForTestSupported();
  }

  ScopedTestFeatureForTestType CreateScopedTestFeatureForTest(bool enabled) {
    return TRuntimeEnabledFeaturesTraits::CreateScopedTestFeatureForTest(
        enabled);
  }

  ScopedTestFeatureImpliedForTestType CreateScopedTestFeatureImpliedForTest(
      bool enabled) {
    return TRuntimeEnabledFeaturesTraits::CreateScopedTestFeatureImpliedForTest(
        enabled);
  }

  ScopedTestFeatureDependentForTestType CreateScopedTestFeatureDependentForTest(
      bool enabled) {
    return TRuntimeEnabledFeaturesTraits::
        CreateScopedTestFeatureDependentForTest(enabled);
  }

  bool TestFeatureEnabled() {
    return TRuntimeEnabledFeaturesTraits::TestFeatureEnabled();
  }

  bool TestFeatureImpliedEnabled() {
    return TRuntimeEnabledFeaturesTraits::TestFeatureImpliedEnabled();
  }

  bool TestFeatureDependentEnabled() {
    return TRuntimeEnabledFeaturesTraits::TestFeatureDependentEnabled();
  }

  bool OriginTrialsSampleAPIEnabledByRuntimeFlag() {
    return TRuntimeEnabledFeaturesTraits::
        OriginTrialsSampleAPIEnabledByRuntimeFlag();
  }

  bool OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag() {
    return TRuntimeEnabledFeaturesTraits::
        OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag();
  }

  bool OriginTrialsSampleAPIDependentEnabledByRuntimeFlag() {
    return TRuntimeEnabledFeaturesTraits::
        OriginTrialsSampleAPIDependentEnabledByRuntimeFlag();
  }

  void SetTestFeatureEnabled(bool enabled) {
    TRuntimeEnabledFeaturesTraits::SetTestFeatureEnabled(enabled);
  }

  void SetTestFeatureImpliedEnabled(bool enabled) {
    TRuntimeEnabledFeaturesTraits::SetTestFeatureImpliedEnabled(enabled);
  }

  void SetTestFeatureDependentEnabled(bool enabled) {
    TRuntimeEnabledFeaturesTraits::SetTestFeatureDependentEnabled(enabled);
  }

  void SetOriginTrialsSampleAPIEnabled(bool enabled) {
    TRuntimeEnabledFeaturesTraits::SetOriginTrialsSampleAPIEnabled(enabled);
  }

  void SetOriginTrialsSampleAPIImpliedEnabled(bool enabled) {
    TRuntimeEnabledFeaturesTraits::SetOriginTrialsSampleAPIImpliedEnabled(
        enabled);
  }

  void SetOriginTrialsSampleAPIDependentEnabled(bool enabled) {
    TRuntimeEnabledFeaturesTraits::SetOriginTrialsSampleAPIDependentEnabled(
        enabled);
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
TYPED_TEST_SUITE_P(AbstractRuntimeEnabledFeaturesTest);

TYPED_TEST_P(AbstractRuntimeEnabledFeaturesTest, Relationship) {
  // Internal status: false, false, false.
  EXPECT_FALSE(this->TestFeatureEnabled());
  EXPECT_FALSE(this->TestFeatureImpliedEnabled());
  EXPECT_FALSE(this->TestFeatureDependentEnabled());

  this->SetTestFeatureEnabled(true);
  // Internal status: true, false, false.
  EXPECT_TRUE(this->TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(this->TestFeatureImpliedEnabled());
  EXPECT_FALSE(this->TestFeatureDependentEnabled());

  this->SetTestFeatureImpliedEnabled(true);
  // Internal status: true, true, false.
  EXPECT_TRUE(this->TestFeatureEnabled());
  EXPECT_TRUE(this->TestFeatureImpliedEnabled());
  EXPECT_FALSE(this->TestFeatureDependentEnabled());

  this->SetTestFeatureDependentEnabled(true);
  // Internal status: true, true, true.
  EXPECT_TRUE(this->TestFeatureEnabled());
  EXPECT_TRUE(this->TestFeatureImpliedEnabled());
  EXPECT_TRUE(this->TestFeatureDependentEnabled());

  this->SetTestFeatureImpliedEnabled(false);
  // Internal status: true, false, true.
  EXPECT_TRUE(this->TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(this->TestFeatureImpliedEnabled());
  EXPECT_TRUE(this->TestFeatureDependentEnabled());

  this->SetTestFeatureEnabled(false);
  // Internal status: false, false, true.
  EXPECT_FALSE(this->TestFeatureEnabled());
  EXPECT_FALSE(this->TestFeatureImpliedEnabled());
  // Depends on TestFeatureImplied.
  EXPECT_FALSE(this->TestFeatureDependentEnabled());

  this->SetTestFeatureImpliedEnabled(true);
  // Internal status: false, true, true.
  EXPECT_FALSE(this->TestFeatureEnabled());
  EXPECT_TRUE(this->TestFeatureImpliedEnabled());
  EXPECT_TRUE(this->TestFeatureDependentEnabled());

  this->SetTestFeatureDependentEnabled(false);
  // Internal status: false, true, false.
  EXPECT_FALSE(this->TestFeatureEnabled());
  EXPECT_TRUE(this->TestFeatureImpliedEnabled());
  EXPECT_FALSE(this->TestFeatureDependentEnabled());
}

TYPED_TEST_P(AbstractRuntimeEnabledFeaturesTest, ScopedForTest) {
  if (!this->ScopedForTestSupported()) {
    return;
  }
  // Internal status: false, false, false.
  EXPECT_FALSE(this->TestFeatureEnabled());
  EXPECT_FALSE(this->TestFeatureImpliedEnabled());
  EXPECT_FALSE(this->TestFeatureDependentEnabled());
  {
    auto f1 = this->CreateScopedTestFeatureForTest(true);
    // Internal status: true, false, false.
    EXPECT_TRUE(this->TestFeatureEnabled());
    // Implied by TestFeature.
    EXPECT_TRUE(this->TestFeatureImpliedEnabled());
    EXPECT_FALSE(this->TestFeatureDependentEnabled());
    {
      auto f2 = this->CreateScopedTestFeatureImpliedForTest(true);
      // Internal status: true, true, false.
      EXPECT_TRUE(this->TestFeatureEnabled());
      EXPECT_TRUE(this->TestFeatureImpliedEnabled());
      EXPECT_FALSE(this->TestFeatureDependentEnabled());
      {
        auto f3 = this->CreateScopedTestFeatureDependentForTest(true);
        // Internal status: true, true, true.
        EXPECT_TRUE(this->TestFeatureEnabled());
        EXPECT_TRUE(this->TestFeatureImpliedEnabled());
        EXPECT_TRUE(this->TestFeatureDependentEnabled());
        {
          auto f3a = this->CreateScopedTestFeatureDependentForTest(false);
          // Internal status: true, true, true.
          EXPECT_TRUE(this->TestFeatureEnabled());
          EXPECT_TRUE(this->TestFeatureImpliedEnabled());
          EXPECT_FALSE(this->TestFeatureDependentEnabled());
        }
        // Internal status: true, true, true.
        EXPECT_TRUE(this->TestFeatureEnabled());
        EXPECT_TRUE(this->TestFeatureImpliedEnabled());
        EXPECT_TRUE(this->TestFeatureDependentEnabled());
      }
    }
    // Internal status: true, false, false.
    EXPECT_TRUE(this->TestFeatureEnabled());
    // Implied by TestFeature.
    EXPECT_TRUE(this->TestFeatureImpliedEnabled());
    EXPECT_FALSE(this->TestFeatureDependentEnabled());
    {
      auto f2a = this->CreateScopedTestFeatureImpliedForTest(false);
      // Internal status: true, false, false.
      EXPECT_TRUE(this->TestFeatureEnabled());
      // Implied by TestFeature.
      EXPECT_TRUE(this->TestFeatureImpliedEnabled());
      EXPECT_FALSE(this->TestFeatureDependentEnabled());
    }
  }
  // Internal status: false, false, false.
  EXPECT_FALSE(this->TestFeatureEnabled());
  EXPECT_FALSE(this->TestFeatureImpliedEnabled());
  EXPECT_FALSE(this->TestFeatureDependentEnabled());
  {
    auto f3 = this->CreateScopedTestFeatureDependentForTest(true);
    // Internal status: false, false, true.
    EXPECT_FALSE(this->TestFeatureEnabled());
    EXPECT_FALSE(this->TestFeatureImpliedEnabled());
    // Depends on TestFeatureImplied.
    EXPECT_FALSE(this->TestFeatureDependentEnabled());
    {
      auto f2 = this->CreateScopedTestFeatureImpliedForTest(true);
      // Internal status: false, true, true.
      EXPECT_FALSE(this->TestFeatureEnabled());
      EXPECT_TRUE(this->TestFeatureImpliedEnabled());
      EXPECT_TRUE(this->TestFeatureDependentEnabled());
      {
        auto f1 = this->CreateScopedTestFeatureForTest(true);
        // Internal status: true, true, true.
        EXPECT_TRUE(this->TestFeatureEnabled());
        EXPECT_TRUE(this->TestFeatureImpliedEnabled());
        EXPECT_TRUE(this->TestFeatureDependentEnabled());
      }
      // Internal status: false, true, true.
      EXPECT_FALSE(this->TestFeatureEnabled());
      EXPECT_TRUE(this->TestFeatureImpliedEnabled());
      EXPECT_TRUE(this->TestFeatureDependentEnabled());
    }
    // Internal status: false, false, true.
    EXPECT_FALSE(this->TestFeatureEnabled());
    EXPECT_FALSE(this->TestFeatureImpliedEnabled());
    // Depends on TestFeatureImplied.
    EXPECT_FALSE(this->TestFeatureDependentEnabled());
    {
      auto f2 = this->CreateScopedTestFeatureImpliedForTest(true);
      // Internal status: false, true, true.
      EXPECT_FALSE(this->TestFeatureEnabled());
      EXPECT_TRUE(this->TestFeatureImpliedEnabled());
      EXPECT_TRUE(this->TestFeatureDependentEnabled());
    }
  }
  // Internal status: false, false, false.
  EXPECT_FALSE(this->TestFeatureEnabled());
  EXPECT_FALSE(this->TestFeatureImpliedEnabled());
  EXPECT_FALSE(this->TestFeatureDependentEnabled());
}

TYPED_TEST_P(AbstractRuntimeEnabledFeaturesTest, BackupRestore) {
  // Internal status: false, false, false.
  EXPECT_FALSE(this->TestFeatureEnabled());
  EXPECT_FALSE(this->TestFeatureImpliedEnabled());
  EXPECT_FALSE(this->TestFeatureDependentEnabled());

  this->SetTestFeatureEnabled(true);
  this->SetTestFeatureDependentEnabled(true);
  // Internal status: true, false, true.
  EXPECT_TRUE(this->TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(this->TestFeatureImpliedEnabled());
  EXPECT_TRUE(this->TestFeatureDependentEnabled());

  RuntimeEnabledFeatures::Backup backup;

  this->SetTestFeatureEnabled(false);
  this->SetTestFeatureImpliedEnabled(true);
  this->SetTestFeatureDependentEnabled(false);
  // Internal status: false, true, false.
  EXPECT_FALSE(this->TestFeatureEnabled());
  EXPECT_TRUE(this->TestFeatureImpliedEnabled());
  EXPECT_FALSE(this->TestFeatureDependentEnabled());

  backup.Restore();
  // Should restore the internal status to: true, false, true.
  EXPECT_TRUE(this->TestFeatureEnabled());
  // Implied by TestFeature.
  EXPECT_TRUE(this->TestFeatureImpliedEnabled());
  EXPECT_TRUE(this->TestFeatureDependentEnabled());

  this->SetTestFeatureEnabled(false);
  // Internal status: false, false, true.
  EXPECT_FALSE(this->TestFeatureEnabled());
  EXPECT_FALSE(this->TestFeatureImpliedEnabled());
  // Depends on TestFeatureImplied.
  EXPECT_FALSE(this->TestFeatureDependentEnabled());
}

// Test setup:
// OriginTrialsSampleAPIImplied   impled_by  \
//                                             OriginTrialsSampleAPI
// OriginTrialsSampleAPIDependent depends_on /
TYPED_TEST_P(AbstractRuntimeEnabledFeaturesTest, OriginTrialsByRuntimeEnabled) {
  // Internal status: false, false, false.
  EXPECT_FALSE(this->OriginTrialsSampleAPIEnabledByRuntimeFlag());
  EXPECT_FALSE(this->OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  EXPECT_FALSE(this->OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());

  this->SetOriginTrialsSampleAPIEnabled(true);
  // Internal status: true, false, false.
  EXPECT_TRUE(this->OriginTrialsSampleAPIEnabledByRuntimeFlag());
  // Implied by OriginTrialsSampleAPI.
  EXPECT_TRUE(this->OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  EXPECT_FALSE(this->OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());

  this->SetOriginTrialsSampleAPIImpliedEnabled(true);
  this->SetOriginTrialsSampleAPIDependentEnabled(true);
  // Internal status: true, true, true.
  EXPECT_TRUE(this->OriginTrialsSampleAPIEnabledByRuntimeFlag());
  EXPECT_TRUE(this->OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  EXPECT_TRUE(this->OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());

  this->SetOriginTrialsSampleAPIEnabled(false);
  // Internal status: false, true, true.
  EXPECT_FALSE(this->OriginTrialsSampleAPIEnabledByRuntimeFlag());
  EXPECT_TRUE(this->OriginTrialsSampleAPIImpliedEnabledByRuntimeFlag());
  // Depends on OriginTrialsSampleAPI.
  EXPECT_FALSE(this->OriginTrialsSampleAPIDependentEnabledByRuntimeFlag());
}

TYPED_TEST_P(AbstractRuntimeEnabledFeaturesTest, CopiedFromBaseFaetureIf) {
  using base::FeatureList;
  const base::Feature& kFeature = features::kTestBlinkFeatureDefault;
  ASSERT_TRUE(FeatureList::IsEnabled(kFeature));
  ASSERT_TRUE(FeatureList::GetInstance()->IsFeatureOverridden(kFeature.name));
  ASSERT_FALSE(FeatureList::GetStateIfOverridden(kFeature));
  WebRuntimeFeatures::UpdateStatusFromBaseFeatures();
  EXPECT_FALSE(RuntimeEnabledFeatures::TestBlinkFeatureDefaultEnabled());
}

REGISTER_TYPED_TEST_SUITE_P(AbstractRuntimeEnabledFeaturesTest,
                            Relationship,
                            ScopedForTest,
                            BackupRestore,
                            OriginTrialsByRuntimeEnabled,
                            CopiedFromBaseFaetureIf);

INSTANTIATE_TYPED_TEST_SUITE_P(Base,
                               AbstractRuntimeEnabledFeaturesTest,
                               RuntimeEnabledFeaturesTestTraits);

INSTANTIATE_TYPED_TEST_SUITE_P(Protected,
                               AbstractRuntimeEnabledFeaturesTest,
                               RuntimeProtectedEnabledFeaturesTestTraits);

}  // namespace blink
