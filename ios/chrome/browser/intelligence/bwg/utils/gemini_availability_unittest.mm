// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_availability.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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
  GeminiAvailabilityResult result = IsGeminiAvailable(
      EntryPoint::ImageContextMenu, profile_.get(), &web_state_);
  EXPECT_TRUE(result.visible);
  EXPECT_TRUE(result.enabled);
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

}  // namespace gemini
