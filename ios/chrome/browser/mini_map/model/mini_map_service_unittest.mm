// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/model/mini_map_service.h"

#import <memory>

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/mini_map/model/mini_map_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class MiniMapServiceTest : public PlatformTest {
 public:
  MiniMapServiceTest() : application_(OCMClassMock([UIApplication class])) {}

  void SetUp() override {
    PlatformTest::SetUp();
    feature_list_.InitAndEnableFeature(kIOSMiniMapUniversalLink);

    TestProfileIOS::Builder test_profile_builder;

    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));

    profile_ = std::move(test_profile_builder).Build();
    OCMStub([application_ sharedApplication]).andReturn(application_);
    mini_map_service_ = MiniMapServiceFactory::GetForProfile(profile_.get());
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(application_);
    [application_ stopMocking];
    PlatformTest::TearDown();
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<MiniMapService> mini_map_service_;
  id application_;
};

// Test that service reports correctly the feature state.
TEST_F(MiniMapServiceTest, TestMiniMapFeatureSetting) {
  // Feature is enabled by default.
  EXPECT_TRUE(profile_->GetSyncablePrefs()->GetBoolean(
      prefs::kIosMiniMapShowNativeMap));
  EXPECT_TRUE(mini_map_service_->IsMiniMapEnabled());

  profile_->GetSyncablePrefs()->SetBoolean(prefs::kIosMiniMapShowNativeMap,
                                           false);
  EXPECT_FALSE(mini_map_service_->IsMiniMapEnabled());
  profile_->GetSyncablePrefs()->SetBoolean(prefs::kIosMiniMapShowNativeMap,
                                           true);
  EXPECT_TRUE(mini_map_service_->IsMiniMapEnabled());
}

// Test that service reports correctly the DSE state.
TEST_F(MiniMapServiceTest, TestMiniMapDSESetting) {
  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
  template_url_service->Load();
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^{
        return template_url_service->loaded();
      }));
  EXPECT_TRUE(mini_map_service_->IsDSEGoogle());

  TemplateURLData not_google_template_url_data;
  not_google_template_url_data.SetURL(
      "https://www.example.com/?q={searchTerms}");
  template_url_service->ApplyDefaultSearchChangeForTesting(
      &not_google_template_url_data, DefaultSearchManager::FROM_USER);

  EXPECT_FALSE(mini_map_service_->IsDSEGoogle());

  TemplateURLData google_template_url_data;
  google_template_url_data.SetURL("https://www.google.com/?q={searchTerms}");
  template_url_service->ApplyDefaultSearchChangeForTesting(
      &google_template_url_data, DefaultSearchManager::FROM_USER);
  EXPECT_TRUE(mini_map_service_->IsDSEGoogle());
}

// Test that service reports correctly the Google Maps is installed.
TEST_F(MiniMapServiceTest, TestMiniMapIsMapsInstalled) {
  EXPECT_FALSE(mini_map_service_->IsGoogleMapsInstalled());
  OCMExpect([application_ canOpenURL:GetGoogleMapsAppURL()]).andReturn(YES);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];
  EXPECT_TRUE(mini_map_service_->IsGoogleMapsInstalled());
  OCMStub([application_ canOpenURL:GetGoogleMapsAppURL()]).andReturn(NO);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];
  EXPECT_FALSE(mini_map_service_->IsGoogleMapsInstalled());
}

// Test that service reports correctly the signed-in state.
TEST_F(MiniMapServiceTest, TestMiniMapIsSignedIn) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(mini_map_service_->IsSignedIn());
  signin::MakePrimaryAccountAvailable(identity_manager, "test@example.com",
                                      signin::ConsentLevel::kSignin);
  EXPECT_TRUE(mini_map_service_->IsSignedIn());
  ClearPrimaryAccount(identity_manager);
  EXPECT_FALSE(mini_map_service_->IsSignedIn());
}
