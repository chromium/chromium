// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_consumer.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_content_entry_point.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Fake BwgService for testing.
class FakeBwgService : public BwgService {
 public:
  FakeBwgService() = default;
  ~FakeBwgService() override = default;
  bool IsProfileEligibleForGemini() override {
    return !ineligibility_reasons_.has_value();
  }
  std::optional<gemini::IneligibilityReasons> GeminiIneligibilityForProfile()
      override {
    return ineligibility_reasons_;
  }
  void SetIsEligible(bool is_eligible) {
    if (is_eligible) {
      ineligibility_reasons_ = std::nullopt;
    } else {
      gemini::IneligibilityReasons reasons;
      reasons.chrome_enterprise = true;
      ineligibility_reasons_ = reasons;
    }
  }
  void SetIneligibilityReasons(
      std::optional<gemini::IneligibilityReasons> reasons) {
    ineligibility_reasons_ = reasons;
  }

 private:
  std::optional<gemini::IneligibilityReasons> ineligibility_reasons_;
};

// Fake consumer that absorbs delegate events during testing.
@interface FakePageActionMenuConsumer : NSObject <PageActionMenuConsumer>
@end
@implementation FakePageActionMenuConsumer
- (void)pageLoadStatusChanged {
}
@end

// Test fixture for PageActionMenuMediator.
class PageActionMenuMediatorTest : public PlatformTest {
 protected:
  PageActionMenuMediatorTest()
      : task_environment_(web::WebTaskEnvironment::MainThreadType::UI) {}
  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry =
        new user_prefs::PrefRegistrySyncable();
    RegisterProfilePrefs(registry.get());
    sync_preferences::PrefServiceMockFactory factory;
    return factory.CreateSyncable(registry.get());
  }
  void SetUp() override {
    PlatformTest::SetUp();
    // Prepare profile components and configure necessary test factories.
    TestProfileIOS::Builder builder;
    builder.SetPrefService(CreatePrefService());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateFactory(
            base::BindOnce(
                [](ProfileIOS* profile)
                    -> std::unique_ptr<AuthenticationServiceDelegate> {
                  return std::make_unique<FakeAuthenticationServiceDelegate>();
                })));

    browser_state_ = std::move(builder).Build();
    auth_service_ =
        AuthenticationServiceFactory::GetForProfile(browser_state_.get());
    pref_service_ = browser_state_->GetPrefs();
    fake_bwg_service_ = std::make_unique<FakeBwgService>();
    web_state_ = std::make_unique<web::FakeWebState>();

    // Create real TabHelper on FakeWebState.
    BwgTabHelper::CreateForWebState(web_state_.get());
    bwg_tab_helper_ = BwgTabHelper::FromWebState(web_state_.get());

    mediator_ =
        [[PageActionMenuMediator alloc] initWithWebState:web_state_.get()
                                   authenticationService:auth_service_
                                      profilePrefService:pref_service_
                                      templateURLService:nil
                                           geminiService:fake_bwg_service_.get()
                                         geminiTabHelper:bwg_tab_helper_
                                     readerModeTabHelper:nil
                                  hostContentSettingsMap:nil];
    fake_consumer_ = [[FakePageActionMenuConsumer alloc] init];
    mediator_.consumer = fake_consumer_;
  }
  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    fake_consumer_ = nil;
    bwg_tab_helper_ = nullptr;
    web_state_.reset();
    fake_bwg_service_.reset();
    pref_service_ = nullptr;
    auth_service_ = nullptr;
    browser_state_.reset();
    PlatformTest::TearDown();
  }
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> browser_state_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<FakeBwgService> fake_bwg_service_;
  raw_ptr<BwgTabHelper> bwg_tab_helper_;
  PageActionMenuMediator* mediator_;
  FakePageActionMenuConsumer* fake_consumer_;
};

// Tests that isGeminiAvailable accurately reflects underlying service and
// tab helper state.
TEST_F(PageActionMenuMediatorTest, IsGeminiAvailable) {
  // Enable PageActionMenu and GeminiFloatyAllPages to simplify availability
  // checks.
  scoped_feature_list_.InitWithFeatures(
      {kPageActionMenu, kGeminiFloatyAllPages}, {});

  // Happy Path: Profile is eligible AND WebState has an eligible URL.
  fake_bwg_service_->SetIsEligible(true);
  web_state_->SetVisibleURL(GURL("https://example.com"));

  EXPECT_TRUE([mediator_ geminiEntryPoint].enabled);

  // Failure state 1: Profile is explicitly not eligible.
  fake_bwg_service_->SetIsEligible(false);
  EXPECT_FALSE([mediator_ geminiEntryPoint].enabled);

  // Failure state 2: Gemini is not available for the current WebState
  // (e.g. invalid URL).
  fake_bwg_service_->SetIsEligible(true);
  web_state_->SetVisibleURL(GURL("chrome://newtab"));  // Ineligible URL
  EXPECT_FALSE([mediator_ geminiEntryPoint].enabled);
}
