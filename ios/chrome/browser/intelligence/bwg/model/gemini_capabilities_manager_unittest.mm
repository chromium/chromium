// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <utility>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager_impl.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class GeminiCapabilitiesManagerTest : public PlatformTest {
 protected:
  GeminiCapabilitiesManagerTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        GeminiServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeGeminiService>();
            }));
    profile_ = std::move(builder).Build();

    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());
    fake_gemini_service_ = static_cast<FakeGeminiService*>(
        GeminiServiceFactory::GetForProfile(profile_.get()));
  }

  void SetUp() override {
    PlatformTest::SetUp();
    // Clear NSUserDefaults before each test.
    NSUserDefaults* defaults = app_group::GetCommonGroupUserDefaults();
    [defaults removeObjectForKey:app_group::kAppSwitcherHashedUserID];
    [defaults removeObjectForKey:app_group::kChromeCapabilitiesPreference];
  }

  void TearDown() override {
    NSUserDefaults* defaults = app_group::GetCommonGroupUserDefaults();
    [defaults removeObjectForKey:app_group::kAppSwitcherHashedUserID];
    [defaults removeObjectForKey:app_group::kChromeCapabilitiesPreference];
    PlatformTest::TearDown();
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<FakeGeminiService> fake_gemini_service_;
};

// Tests that when the feature is disabled, all capabilities are cleared.
TEST_F(GeminiCapabilitiesManagerTest, FeatureDisabledClearsCapabilities) {
  scoped_feature_list_.InitAndDisableFeature(kAppSwitcherAISummarization);

  // Pre-populate defaults to verify they get cleared.
  NSUserDefaults* defaults = app_group::GetCommonGroupUserDefaults();
  [defaults setObject:@"fake_hashed_id"
               forKey:app_group::kAppSwitcherHashedUserID];
  [defaults setObject:@{
    app_group::kChromeSupportsAISummarizationCapability : @YES,
    app_group::kChromeUserIsEligibleForGeminiCapability : @YES
  }
               forKey:app_group::kChromeCapabilitiesPreference];

  // Constructor automatically calls UpdateCapabilities()!
  GeminiCapabilitiesManagerImpl manager(profile_.get(), auth_service_,
                                        fake_gemini_service_);

  EXPECT_NSEQ(nil, [defaults objectForKey:app_group::kAppSwitcherHashedUserID]);
  NSDictionary* capabilities =
      [defaults dictionaryForKey:app_group::kChromeCapabilitiesPreference];
  EXPECT_NSEQ(
      nil, capabilities[app_group::kChromeSupportsAISummarizationCapability]);
  EXPECT_NSEQ(
      nil, capabilities[app_group::kChromeUserIsEligibleForGeminiCapability]);
}

// Tests that when the feature is enabled and there is no signed-in user,
// SupportsAISummarization is YES, UserIsEligibleForGemini is NO, and
// HashedUserID is cleared.
TEST_F(GeminiCapabilitiesManagerTest, FeatureEnabledNoUser) {
  scoped_feature_list_.InitWithFeatures(
      {kPageActionMenu, kAppSwitcherAISummarization}, {});

  // Constructor automatically calls UpdateCapabilities()!
  GeminiCapabilitiesManagerImpl manager(profile_.get(), auth_service_,
                                        fake_gemini_service_);
  fake_gemini_service_->SetIsEligible(false);

  NSUserDefaults* defaults = app_group::GetCommonGroupUserDefaults();
  EXPECT_NSEQ(nil, [defaults objectForKey:app_group::kAppSwitcherHashedUserID]);

  NSDictionary* capabilities =
      [defaults dictionaryForKey:app_group::kChromeCapabilitiesPreference];
  EXPECT_TRUE([capabilities[app_group::kChromeSupportsAISummarizationCapability]
      boolValue]);
  EXPECT_FALSE(
      [capabilities[app_group::kChromeUserIsEligibleForGeminiCapability]
          boolValue]);
}

// Tests that when the feature is enabled and there is a signed-in user,
// HashedUserID is set to the user's hashed GAIA ID.
TEST_F(GeminiCapabilitiesManagerTest, FeatureEnabledWithUser) {
  scoped_feature_list_.InitWithFeatures(
      {kPageActionMenu, kAppSwitcherAISummarization}, {});

  // Sign in a fake identity.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(identity);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  signin::AccountAvailabilityOptionsBuilder options_builder;
  options_builder.AsPrimary(signin::ConsentLevel::kSignin);
  options_builder.WithGaiaId(identity.gaiaId);
  signin::MakeAccountAvailable(
      identity_manager,
      options_builder.Build(base::SysNSStringToUTF8(identity.userEmail)));

  auth_service_->SignIn(identity, signin_metrics::AccessPoint::kStartPage);

  // Constructor automatically calls UpdateCapabilities()!
  GeminiCapabilitiesManagerImpl manager(profile_.get(), auth_service_,
                                        fake_gemini_service_);
  fake_gemini_service_->SetIsEligible(true);

  NSUserDefaults* defaults = app_group::GetCommonGroupUserDefaults();
  NSString* hashed_uid =
      [defaults stringForKey:app_group::kAppSwitcherHashedUserID];
  // Verify that the hashed GAIA ID is correctly set.
  EXPECT_NSEQ(identity.hashedGaiaID, hashed_uid);

  NSDictionary* capabilities =
      [defaults dictionaryForKey:app_group::kChromeCapabilitiesPreference];
  EXPECT_TRUE([capabilities[app_group::kChromeSupportsAISummarizationCapability]
      boolValue]);
  EXPECT_TRUE([capabilities[app_group::kChromeUserIsEligibleForGeminiCapability]
      boolValue]);
}

}  // namespace
