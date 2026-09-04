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
#import "ios/chrome/app/startup/app_startup_utils.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager_impl.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
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
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_);
    fake_gemini_service_ = static_cast<FakeGeminiService*>(
        GeminiServiceFactory::GetForProfile(profile_));
  }

  void SetUp() override {
    PlatformTest::SetUp();
    // Clear NSUserDefaults before each test.
    NSUserDefaults* defaults = app_group::GetCommonGroupUserDefaults();
    [defaults removeObjectForKey:app_group::kChromeCapabilitiesPreference];
  }

  void TearDown() override {
    auth_service_ = nullptr;
    fake_gemini_service_ = nullptr;
    profile_ = nullptr;
    NSUserDefaults* defaults = app_group::GetCommonGroupUserDefaults();
    [defaults removeObjectForKey:app_group::kChromeCapabilitiesPreference];
    PlatformTest::TearDown();
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<FakeGeminiService> fake_gemini_service_;
};


// Tests that when the feature is enabled and there is no signed-in user,
// SupportsAISummarization is YES and UserIsEligibleForGemini is NO.
TEST_F(GeminiCapabilitiesManagerTest, FeatureEnabledNoUser) {
  scoped_feature_list_.InitWithFeatures(
      {kPageActionMenu, kAppSwitcherAISummarization}, {});

  GeminiCapabilitiesManagerImpl manager(profile_, auth_service_,
                                        fake_gemini_service_);
  manager.UpdateCapabilities();
  fake_gemini_service_->SetIsEligible(false);

  NSUserDefaults* defaults = app_group::GetCommonGroupUserDefaults();
  NSDictionary* capabilities =
      [defaults dictionaryForKey:app_group::kChromeCapabilitiesPreference];
  EXPECT_TRUE([capabilities[app_group::kChromeSupportsAISummarizationCapability]
      boolValue]);
  EXPECT_FALSE(
      [capabilities[app_group::kChromeUserIsEligibleForGeminiCapability]
          boolValue]);
}

// Tests that when the feature is enabled and there is a signed-in user,
// SupportsAISummarization is YES and UserIsEligibleForGemini is YES.
TEST_F(GeminiCapabilitiesManagerTest, FeatureEnabledWithUser) {
  scoped_feature_list_.InitWithFeatures(
      {kPageActionMenu, kAppSwitcherAISummarization}, {});

  // Sign in a fake identity.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(identity);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  signin::AccountAvailabilityOptionsBuilder options_builder;
  options_builder.AsPrimary(signin::ConsentLevel::kSignin);
  options_builder.WithGaiaId(identity.gaiaId);
  signin::MakeAccountAvailable(
      identity_manager,
      options_builder.Build(base::SysNSStringToUTF8(identity.userEmail)));

  auth_service_->SignIn(identity, signin_metrics::AccessPoint::kStartPage);

  GeminiCapabilitiesManagerImpl manager(profile_, auth_service_,
                                        fake_gemini_service_);
  manager.UpdateCapabilities();
  fake_gemini_service_->SetIsEligible(true);

  NSUserDefaults* defaults = app_group::GetCommonGroupUserDefaults();
  NSDictionary* capabilities =
      [defaults dictionaryForKey:app_group::kChromeCapabilitiesPreference];
  EXPECT_TRUE([capabilities[app_group::kChromeSupportsAISummarizationCapability]
      boolValue]);
  EXPECT_TRUE([capabilities[app_group::kChromeUserIsEligibleForGeminiCapability]
      boolValue]);
}

// Tests that after `UpdateCapabilities` sets Gemini capabilities, a subsequent
// app restart emulation (invoking `MockSaveFieldTrialValuesForGroupApp`)
// preserves the existing Gemini capabilities alongside newly saved field trial
// values.
TEST_F(GeminiCapabilitiesManagerTest, PreservesExistingCapabilitiesOnRestart) {
  scoped_feature_list_.InitWithFeatures(
      {kPageActionMenu, kAppSwitcherAISummarization}, {});

  GeminiCapabilitiesManagerImpl manager(profile_, auth_service_,
                                        fake_gemini_service_);
  manager.UpdateCapabilities();

  NSUserDefaults* defaults = app_group::GetCommonGroupUserDefaults();
  NSDictionary* capabilitiesBeforeRestart =
      [defaults dictionaryForKey:app_group::kChromeCapabilitiesPreference];
  ASSERT_TRUE(capabilitiesBeforeRestart);
  EXPECT_TRUE([capabilitiesBeforeRestart
          [app_group::kChromeSupportsAISummarizationCapability] boolValue]);

  // Emulate app restart / startup capability sync.
  SaveFieldTrialValuesForGroupApp();

  NSDictionary* capabilitiesAfterRestart =
      [defaults dictionaryForKey:app_group::kChromeCapabilitiesPreference];
  ASSERT_TRUE(capabilitiesAfterRestart);

  // Verify Gemini capability was preserved across restart.
  EXPECT_TRUE([capabilitiesAfterRestart
          [app_group::kChromeSupportsAISummarizationCapability] boolValue]);

  // Verify non-Gemini capabilities were also saved.
  EXPECT_NSEQ(@YES, capabilitiesAfterRestart
                        [app_group::kChromeShowDefaultBrowserPromoCapability]);
}

// Tests that when the feature is disabled,
// `SaveFieldTrialValuesForGroupApp` cleans up stale Gemini capabilities
// from NSUserDefaults.
TEST_F(GeminiCapabilitiesManagerTest, ClearsCapabilitiesWhenFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kAppSwitcherAISummarization);

  // Pre-populate defaults to verify they get cleared when feature is disabled.
  NSUserDefaults* defaults = app_group::GetCommonGroupUserDefaults();
  [defaults setObject:@{
    app_group::kChromeSupportsAISummarizationCapability : @YES,
    app_group::kChromeUserIsEligibleForGeminiCapability : @YES
  }
               forKey:app_group::kChromeCapabilitiesPreference];

  SaveFieldTrialValuesForGroupApp();

  NSDictionary* capabilities =
      [defaults dictionaryForKey:app_group::kChromeCapabilitiesPreference];
  EXPECT_NSEQ(
      nil, capabilities[app_group::kChromeSupportsAISummarizationCapability]);
  EXPECT_NSEQ(
      nil, capabilities[app_group::kChromeUserIsEligibleForGeminiCapability]);
}

}  // namespace
