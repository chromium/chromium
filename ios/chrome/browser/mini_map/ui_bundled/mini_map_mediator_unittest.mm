// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/ui_bundled/mini_map_mediator.h"

#import "base/ios/ios_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/mini_map/ui_bundled/mini_map_mediator_delegate.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/common/features.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class MiniMapMediatorTest : public PlatformTest {
 protected:
  MiniMapMediatorTest() {
    TestProfileIOS::Builder builder;
    builder.SetPrefService(CreatePrefService());
    profile_ = std::move(builder).Build();

    delegate_ = OCMStrictProtocolMock(@protocol(MiniMapMediatorDelegate));

    mediator_ = [[MiniMapMediator alloc] initWithPrefs:profile_->GetPrefs()
                                              webState:nullptr];
    mediator_.delegate = delegate_;
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    user_prefs::PrefRegistrySyncable* registry = prefs->registry();
    RegisterProfilePrefs(registry);
    return prefs;
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
    PlatformTest::TearDown();
  }

 protected:
  base::test::TaskEnvironment environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  id delegate_;
  MiniMapMediator* mediator_;
};

// Tests that consent screen is not triggered if not needed.
TEST_F(MiniMapMediatorTest, TestNoConsentNeeded) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(web::features::kOneTapForMaps);

  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  OCMExpect([delegate_ showMapWithIPH:NO]);
  [mediator_ userInitiatedMiniMapConsentRequired:NO];
}

// Tests that settings are updated correctly after user consents.
TEST_F(MiniMapMediatorTest, TestUserConsents) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(web::features::kOneTapForMaps);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  OCMExpect([delegate_ showConsentInterstitial]);
  [mediator_ userInitiatedMiniMapConsentRequired:YES];
  OCMExpect([delegate_ showMapWithIPH:NO]);
  [mediator_ userConsented];
  environment_.RunUntilIdle();
  EXPECT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kDetectAddressesAccepted));
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kDetectAddressesEnabled));
}

// Tests that settings are updated correctly after user declines.
TEST_F(MiniMapMediatorTest, TestUserDeclines) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(web::features::kOneTapForMaps);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  OCMExpect([delegate_ showConsentInterstitial]);
  [mediator_ userInitiatedMiniMapConsentRequired:YES];
  OCMExpect([delegate_ dismissConsentInterstitialWithCompletion:[OCMArg any]]);
  [mediator_ userDeclined];
  environment_.RunUntilIdle();
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kDetectAddressesAccepted));
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kDetectAddressesEnabled));
}

// Tests that consent is presented if it is forced.
TEST_F(MiniMapMediatorTest, TestUserConsentForced) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams feature_parameters{
      {web::features::kOneTapForMapsConsentModeParamTitle,
       web::features::kOneTapForMapsConsentModeForcedParam}};
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      web::features::kOneTapForMaps, feature_parameters);

  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, true);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  OCMExpect([delegate_ showConsentInterstitial]);
  [mediator_ userInitiatedMiniMapConsentRequired:YES];
  OCMExpect([delegate_ showMapWithIPH:NO]);
  [mediator_ userConsented];
  environment_.RunUntilIdle();
  EXPECT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kDetectAddressesAccepted));
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kDetectAddressesEnabled));
}

// Tests that consent screen is not triggered but IPH is displayed.
TEST_F(MiniMapMediatorTest, TestConsentIPH) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams feature_parameters{
      {web::features::kOneTapForMapsConsentModeParamTitle,
       web::features::kOneTapForMapsConsentModeIPHParam}};
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      web::features::kOneTapForMaps, feature_parameters);

  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  OCMExpect([delegate_ showMapWithIPH:YES]);
  [mediator_ userInitiatedMiniMapConsentRequired:YES];

  environment_.RunUntilIdle();
  EXPECT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kDetectAddressesAccepted));
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kDetectAddressesEnabled));
}

// Tests that consent screen is not triggered if not needed.
TEST_F(MiniMapMediatorTest, TestConsentDisabled) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams feature_parameters{
      {web::features::kOneTapForMapsConsentModeParamTitle,
       web::features::kOneTapForMapsConsentModeDisabledParam}};
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      web::features::kOneTapForMaps, feature_parameters);

  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  OCMExpect([delegate_ showMapWithIPH:NO]);
  [mediator_ userInitiatedMiniMapConsentRequired:NO];
}
