// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_availability.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/mock_network_change_notifier.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace ios::provider {
void SetMockFeatureModeDisabledByQuota(bool disabled);
void SetMockRefillDateForFeatureMode(NSDate* date);
}  // namespace ios::provider

namespace {
constexpr NSTimeInterval kArbitraryTimestamp = 1777998600;
}  // namespace

namespace gemini {

class GeminiAvailabilityTest : public PlatformTest {
 public:
  GeminiAvailabilityTest() {
    feature_list_.InitAndEnableFeature(kPageActionMenu);
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(&IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        GeminiServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeGeminiService>();
            }));
    profile_ = std::move(builder).Build();
    fake_gemini_service_ = static_cast<FakeGeminiService*>(
        GeminiServiceFactory::GetForProfile(profile_.get()));
    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());

    web_state_.SetBrowserState(profile_.get());
    web_state_.WasShown();
    web_state_.SetCurrentURL(GURL("https://www.google.com"));
    GeminiTabHelper::CreateForWebState(&web_state_);
  }

  void SignIn(id<SystemIdentity> identity) {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    signin::AccountAvailabilityOptionsBuilder builder;
    builder.WithGaiaId(identity.gaiaId)
        .AsPrimary(signin::ConsentLevel::kSignin);
    signin::MakeAccountAvailable(
        identity_manager,
        builder.Build(base::SysNSStringToUTF8(identity.userEmail)));

    auth_service_->SignIn(identity, signin_metrics::AccessPoint::kStartPage);
  }

  void TearDown() override {
    ios::provider::SetMockFeatureModeDisabledByQuota(false);
    ios::provider::SetMockRefillDateForFeatureMode(nil);
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<FakeGeminiService> fake_gemini_service_;
  raw_ptr<AuthenticationService> auth_service_;
  web::FakeWebState web_state_;
};

TEST_F(GeminiAvailabilityTest, PageActionMenuAvailable) {
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::AIHub, profile_.get(), &web_state_);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
  EXPECT_FALSE(result.ineligibility_reasons.has_value());
}

TEST_F(GeminiAvailabilityTest, PageActionMenuIneligibleProfile) {
  fake_gemini_service_->SetIsEligible(false);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::AIHub, profile_.get(), &web_state_);
  EXPECT_FALSE(result.visible);
  EXPECT_FALSE(result.enabled);
  ASSERT_TRUE(result.ineligibility_reasons.has_value());
  EXPECT_TRUE(result.ineligibility_reasons->chrome_enterprise);
}

TEST_F(GeminiAvailabilityTest, ContextualEntryPointAllowed) {
  fake_gemini_service_->SetIsEligible(true);
  gemini::test::SetUpEligibleAccount(profile_.get());

  GeminiAvailabilityResult result = IsGeminiAvailable(
      EntryPoint::ImageContextMenu, profile_.get(), &web_state_);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
  EXPECT_FALSE(result.disabled_reason.has_value());
}

TEST_F(GeminiAvailabilityTest, ImageContextMenuQuotaReached) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kPageActionMenu, kGeminiAureus}, {});
  fake_gemini_service_->SetIsEligible(true);
  gemini::test::SetUpEligibleAccount(profile_.get());

  NSDate* mock_date =
      [NSDate dateWithTimeIntervalSince1970:kArbitraryTimestamp];
  ios::provider::SetMockRefillDateForFeatureMode(mock_date);
  ios::provider::SetMockFeatureModeDisabledByQuota(true);

  GeminiAvailabilityResult result = IsGeminiAvailable(
      EntryPoint::ImageContextMenu, profile_.get(), &web_state_);
  EXPECT_TRUE(result.visible);
  EXPECT_FALSE(result.enabled);
  EXPECT_EQ(result.disabled_reason, EntryPointDisabledReason::kQuotaExhausted);
  EXPECT_NE(result.disabled_reason_subtitle, nil);
  EXPECT_TRUE([result.disabled_reason_subtitle
      containsString:@"Images will be available again when your limit resets"]);
}

TEST_F(GeminiAvailabilityTest, ImageRemixIPHQuotaReached) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kPageActionMenu, kGeminiAureus}, {});
  fake_gemini_service_->SetIsEligible(true);
  gemini::test::SetUpEligibleAccount(profile_.get());

  NSDate* mock_date =
      [NSDate dateWithTimeIntervalSince1970:kArbitraryTimestamp];
  ios::provider::SetMockRefillDateForFeatureMode(mock_date);
  ios::provider::SetMockFeatureModeDisabledByQuota(true);

  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::ImageRemixIPH, profile_.get(), &web_state_);
  EXPECT_FALSE(result.visible);
  EXPECT_FALSE(result.enabled);
  EXPECT_EQ(result.disabled_reason, EntryPointDisabledReason::kQuotaExhausted);
  EXPECT_NE(result.disabled_reason_subtitle, nil);
  EXPECT_TRUE([result.disabled_reason_subtitle
      containsString:@"Images will be available again when your limit resets"]);
}

TEST_F(GeminiAvailabilityTest, HighLevelFlagDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(kPageActionMenu);
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::AIHub, profile_.get(), &web_state_);
  EXPECT_FALSE(result.visible);
  EXPECT_FALSE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, AppSwitcherAvailable) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({kPageActionMenu, kAppSwitcherAISummarization},
                                 {});
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result = IsGeminiAvailable(
      EntryPoint::AppSwitcherAISummarization, profile_.get(), nullptr);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, AppSwitcherFlagDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({kPageActionMenu},
                                 {kAppSwitcherAISummarization});
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result = IsGeminiAvailable(
      EntryPoint::AppSwitcherAISummarization, profile_.get(), nullptr);
  EXPECT_FALSE(result.visible);
  EXPECT_FALSE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, AtMemorySearchAvailable) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({kPageActionMenu}, {});
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::AtMemorySearch, profile_.get(), nullptr);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, NullWebStateForTabSurface) {
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::AIHub, profile_.get(), nullptr);
  EXPECT_FALSE(result.visible);
  EXPECT_FALSE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, IneligibleWebStateURL) {
  fake_gemini_service_->SetIsEligible(true);
  web_state_.SetCurrentURL(GURL("chrome://newtab"));
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::AIHub, profile_.get(), &web_state_);
  EXPECT_FALSE(result.visible);
  EXPECT_FALSE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, EditMenuAvailable) {
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::EditMenu, profile_.get(), &web_state_);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, AppSwitcherIneligibleProfile) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({kPageActionMenu, kAppSwitcherAISummarization},
                                 {});
  fake_gemini_service_->SetIsEligible(false);
  GeminiAvailabilityResult result = IsGeminiAvailable(
      EntryPoint::AppSwitcherAISummarization, profile_.get(), nullptr);
  EXPECT_FALSE(result.visible);
  EXPECT_FALSE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, NullProfile) {
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::AIHub, nullptr, &web_state_);
  EXPECT_FALSE(result.visible);
  EXPECT_FALSE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, ToolbarProfileInferredFromWebState) {
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::Toolbar, nullptr, &web_state_);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, EntryPointUnknown) {
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::Unknown, profile_.get(), &web_state_);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, ToolbarAvailable) {
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::Toolbar, profile_.get(), &web_state_);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
}

TEST_F(GeminiAvailabilityTest, ToolbarVisibleWhenSignedOutAndPolicyAllowed) {
  fake_gemini_service_->SetIsEligible(false);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::Toolbar, profile_.get(), &web_state_,
                        auth_service_, profile_->GetPrefs());
  // Visible and enabled for signed-out users when policy allows, so tapping it
  // can trigger the sign-in flow.
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
}

// Tests that the Toolbar assistant button remains visible and enabled on
// web states with ineligible URLs.
TEST_F(GeminiAvailabilityTest, TestToolbarAvailableOnIneligibleWebState) {
  fake_gemini_service_->SetIsEligible(true);
  web_state_.SetCurrentURL(GURL("chrome://newtab"));
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::Toolbar, profile_.get(), &web_state_);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
}

// Tests that the AppBar assistant button is visible and enabled when eligible.
TEST_F(GeminiAvailabilityTest, TestAppBarAvailable) {
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::AppBar, profile_.get(), &web_state_);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
}

// Tests that the AppBar assistant button remains visible and enabled even
// when web state is nullptr or on ineligible URL.
TEST_F(GeminiAvailabilityTest, TestAppBarAvailableOnIneligibleWebState) {
  fake_gemini_service_->SetIsEligible(true);
  GeminiAvailabilityResult result =
      IsGeminiAvailable(EntryPoint::AppBar, profile_.get(), nullptr);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
}

// Tests that bar surfaces (Toolbar and AppBar) are not visible or enabled in
// EEA or Japan countries.
TEST_F(GeminiAvailabilityTest, TestBarExcludedInEEAOrJapan) {
  fake_gemini_service_->SetIsEligible(true);
  IOSChromeScopedTestingVariationsService variations_service;
  variations_service.Get()->OverrideStoredPermanentCountry("fr");

  GeminiAvailabilityResult toolbar_result =
      IsGeminiAvailable(EntryPoint::Toolbar, profile_.get(), &web_state_);
  EXPECT_FALSE(toolbar_result.visible);
  EXPECT_FALSE(toolbar_result.enabled);

  GeminiAvailabilityResult app_bar_result =
      IsGeminiAvailable(EntryPoint::AppBar, profile_.get(), &web_state_);
  EXPECT_FALSE(app_bar_result.visible);
  EXPECT_FALSE(app_bar_result.enabled);

  variations_service.Get()->OverrideStoredPermanentCountry("jp");
  toolbar_result =
      IsGeminiAvailable(EntryPoint::Toolbar, profile_.get(), &web_state_);
  EXPECT_FALSE(toolbar_result.visible);
  EXPECT_FALSE(toolbar_result.enabled);

  app_bar_result =
      IsGeminiAvailable(EntryPoint::AppBar, profile_.get(), &web_state_);
  EXPECT_FALSE(app_bar_result.visible);
  EXPECT_FALSE(app_bar_result.enabled);
}

// Tests that bar surfaces are visible and enabled for unverified accounts
// with persistent auth errors when sign-in is allowed.
TEST_F(GeminiAvailabilityTest, TestBarVisibleAndEnabledForUnverifiedAccount) {
  fake_gemini_service_->SetIsEligible(false);
  id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
  SignIn(identity);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  GeminiAvailabilityResult toolbar_result =
      IsGeminiAvailable(EntryPoint::Toolbar, profile_.get(), &web_state_,
                        auth_service_, profile_->GetPrefs());
  EXPECT_TRUE(toolbar_result.visible);
  EXPECT_TRUE(toolbar_result.enabled);

  GeminiAvailabilityResult app_bar_result =
      IsGeminiAvailable(EntryPoint::AppBar, profile_.get(), &web_state_,
                        auth_service_, profile_->GetPrefs());
  EXPECT_TRUE(app_bar_result.visible);
  EXPECT_TRUE(app_bar_result.enabled);
}

// Tests that bar surfaces default to eligible when workspace policy check is
// pending.
TEST_F(GeminiAvailabilityTest, TestBarWorkspacePolicyPendingAllowed) {
  fake_gemini_service_->SetIsEligible(true);
  SignIn([FakeSystemIdentity fakeIdentity1]);
  fake_gemini_service_->SetWorkspacePolicyCheckPending(true);

  GeminiAvailabilityResult toolbar_result =
      IsGeminiAvailable(EntryPoint::Toolbar, profile_.get(), &web_state_,
                        auth_service_, profile_->GetPrefs());
  EXPECT_TRUE(toolbar_result.visible);
  EXPECT_TRUE(toolbar_result.enabled);

  GeminiAvailabilityResult app_bar_result =
      IsGeminiAvailable(EntryPoint::AppBar, profile_.get(), &web_state_,
                        auth_service_, profile_->GetPrefs());
  EXPECT_TRUE(app_bar_result.visible);
  EXPECT_TRUE(app_bar_result.enabled);
}

// Tests that bar surfaces are ineligible when online and workspace policy
// explicitly restricts access.
TEST_F(GeminiAvailabilityTest, TestBarWorkspacePolicyRestrictedOnline) {
  fake_gemini_service_->SetIsEligible(true);
  SignIn([FakeSystemIdentity fakeIdentity1]);
  fake_gemini_service_->SetWorkspacePolicyCheckPending(false);

  gemini::IneligibilityReasons reasons;
  reasons.workspace = true;
  fake_gemini_service_->SetIneligibilityReasons(reasons);

  GeminiAvailabilityResult toolbar_result =
      IsGeminiAvailable(EntryPoint::Toolbar, profile_.get(), &web_state_,
                        auth_service_, profile_->GetPrefs());
  EXPECT_FALSE(toolbar_result.visible);
  EXPECT_FALSE(toolbar_result.enabled);

  GeminiAvailabilityResult app_bar_result =
      IsGeminiAvailable(EntryPoint::AppBar, profile_.get(), &web_state_,
                        auth_service_, profile_->GetPrefs());
  EXPECT_FALSE(app_bar_result.visible);
  EXPECT_FALSE(app_bar_result.enabled);
}

// Tests that bar surfaces optimistically fall back to eligible when offline,
// even if workspace policy was restricted.
TEST_F(GeminiAvailabilityTest, TestBarWorkspacePolicyRestrictedOffline) {
  std::unique_ptr<net::test::MockNetworkChangeNotifier> mock_network =
      net::test::MockNetworkChangeNotifier::Create();
  mock_network->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  fake_gemini_service_->SetIsEligible(true);
  SignIn([FakeSystemIdentity fakeIdentity1]);
  fake_gemini_service_->SetWorkspacePolicyCheckPending(false);

  gemini::IneligibilityReasons reasons;
  reasons.workspace = true;
  fake_gemini_service_->SetIneligibilityReasons(reasons);

  GeminiAvailabilityResult toolbar_result =
      IsGeminiAvailable(EntryPoint::Toolbar, profile_.get(), &web_state_,
                        auth_service_, profile_->GetPrefs());
  EXPECT_TRUE(toolbar_result.visible);
  EXPECT_TRUE(toolbar_result.enabled);

  GeminiAvailabilityResult app_bar_result =
      IsGeminiAvailable(EntryPoint::AppBar, profile_.get(), &web_state_,
                        auth_service_, profile_->GetPrefs());
  EXPECT_TRUE(app_bar_result.visible);
  EXPECT_TRUE(app_bar_result.enabled);
}

// Tests that bar surfaces are disabled when enterprise policy disables GenAi
// or Gemini.
TEST_F(GeminiAvailabilityTest, TestBarDisabledByPolicy) {
  fake_gemini_service_->SetIsEligible(true);
  profile_->GetPrefs()->SetInteger(
      prefs::kGenAiEnabledByPolicy,
      static_cast<int>(gemini::GenAiDefaultSettingsPolicy::kNotAllowed));

  GeminiAvailabilityResult toolbar_result =
      IsGeminiAvailable(EntryPoint::Toolbar, profile_.get(), &web_state_);
  EXPECT_FALSE(toolbar_result.visible);
  EXPECT_FALSE(toolbar_result.enabled);

  GeminiAvailabilityResult app_bar_result =
      IsGeminiAvailable(EntryPoint::AppBar, profile_.get(), &web_state_);
  EXPECT_FALSE(app_bar_result.visible);
  EXPECT_FALSE(app_bar_result.enabled);
}

}  // namespace gemini
