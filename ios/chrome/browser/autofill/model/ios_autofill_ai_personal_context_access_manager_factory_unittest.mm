// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_autofill_ai_personal_context_access_manager_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/personal_context/core/personal_context_features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Test fixture for IOSAutofillAiPersonalContextAccessManagerFactory.
class IOSAutofillAiPersonalContextAccessManagerFactoryTest
    : public PlatformTest {
 public:
  IOSAutofillAiPersonalContextAccessManagerFactoryTest() = default;

 protected:
  web::WebTaskEnvironment task_environment_;
};

// Tests that AutofillAiPersonalContextAccessManager is instantiated for a
// regular profile when both the kAutofillAmbientAutofill and kPersonalContext
// feature flags are enabled.
TEST_F(IOSAutofillAiPersonalContextAccessManagerFactoryTest,
       CheckServiceNotNullWhenFeaturesEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{autofill::features::kAutofillAmbientAutofill,
                            personal_context::features::kPersonalContext},
      /*disabled_features=*/{});
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  EXPECT_NE(nullptr,
            IOSAutofillAiPersonalContextAccessManagerFactory::GetForProfile(
                profile.get()));
}

// Tests that AutofillAiPersonalContextAccessManager is not instantiated
// (returns nullptr) when the kAutofillAmbientAutofill feature flag is disabled.
TEST_F(IOSAutofillAiPersonalContextAccessManagerFactoryTest,
       CheckServiceNullWhenAmbientAutofillDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{personal_context::features::kPersonalContext},
      /*disabled_features=*/{autofill::features::kAutofillAmbientAutofill});
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  EXPECT_EQ(nullptr,
            IOSAutofillAiPersonalContextAccessManagerFactory::GetForProfile(
                profile.get()));
}

// Tests that AutofillAiPersonalContextAccessManager is not instantiated
// (returns nullptr) when the kPersonalContext feature flag is disabled.
TEST_F(IOSAutofillAiPersonalContextAccessManagerFactoryTest,
       CheckServiceNullWhenPersonalContextDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{autofill::features::kAutofillAmbientAutofill},
      /*disabled_features=*/{personal_context::features::kPersonalContext});
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  EXPECT_EQ(nullptr,
            IOSAutofillAiPersonalContextAccessManagerFactory::GetForProfile(
                profile.get()));
}

// Tests that AutofillAiPersonalContextAccessManager is not created for an
// Off-The-Record (Incognito) profile.
TEST_F(IOSAutofillAiPersonalContextAccessManagerFactoryTest,
       CheckServiceNullForIncognitoProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{autofill::features::kAutofillAmbientAutofill,
                            personal_context::features::kPersonalContext},
      /*disabled_features=*/{});
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
  ProfileIOS* otr_profile = profile->GetOffTheRecordProfile();

  EXPECT_EQ(nullptr,
            IOSAutofillAiPersonalContextAccessManagerFactory::GetForProfile(
                otr_profile));
}

}  // namespace
