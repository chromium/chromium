// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/lens/lens_overlay_permission_utils.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_consumer.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_content_entry_point.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_feature.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/web/model/blocked_popup_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

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
  void SetUp() override {
    PlatformTest::SetUp();

    // Prepare profile components and configure necessary test factories.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        feature_engagement::TrackerFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateFactory(
            base::BindOnce(
                [](ProfileIOS* profile)
                    -> std::unique_ptr<AuthenticationServiceDelegate> {
                  return std::make_unique<FakeAuthenticationServiceDelegate>();
                })));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());

    browser_state_ = std::move(builder).Build();

    // Fetch required services from the profile.
    auth_service_ =
        AuthenticationServiceFactory::GetForProfile(browser_state_.get());
    pref_service_ = browser_state_->GetPrefs();
    ASSERT_TRUE(pref_service_);
    fake_gemini_service_ = std::make_unique<FakeGeminiService>();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(browser_state_.get());

    // Set up search engines environment and content settings map.
    TemplateURLService* template_url_service =
        search_engines_test_environment_.template_url_service();
    settings_map_ =
        ios::HostContentSettingsMapFactory::GetForProfile(browser_state_.get());
    ASSERT_TRUE(settings_map_);

    // Attach required tab helpers to the fake web state.
    BwgTabHelper::CreateForWebState(web_state_.get());
    bwg_tab_helper_ = BwgTabHelper::FromWebState(web_state_.get());

    // Initialize the default mediator with the fake web state and services.
    mediator_ = [[PageActionMenuMediator alloc]
              initWithWebState:web_state_.get()
         authenticationService:auth_service_
            profilePrefService:pref_service_
            templateURLService:template_url_service
                 geminiService:fake_gemini_service_.get()
               geminiTabHelper:bwg_tab_helper_
           readerModeTabHelper:nil
        hostContentSettingsMap:settings_map_];
    fake_consumer_ = [[FakePageActionMenuConsumer alloc] init];
    mediator_.consumer = fake_consumer_;
  }
  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    fake_consumer_ = nil;
    bwg_tab_helper_ = nullptr;
    web_state_.reset();
    fake_gemini_service_.reset();
    settings_map_ = nullptr;
    pref_service_ = nullptr;
    auth_service_ = nullptr;
    browser_state_.reset();
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> browser_state_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<HostContentSettingsMap> settings_map_ = nullptr;
  std::unique_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<FakeGeminiService> fake_gemini_service_;
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
  fake_gemini_service_->SetIsEligible(true);
  web_state_->SetVisibleURL(GURL("https://example.com"));

  EXPECT_TRUE([mediator_ geminiEntryPoint].enabled);

  // Failure state 1: Profile is explicitly not eligible.
  fake_gemini_service_->SetIsEligible(false);
  EXPECT_FALSE([mediator_ geminiEntryPoint].enabled);

  // Failure state 2: Gemini is not available for the current WebState
  // (e.g. invalid URL).
  fake_gemini_service_->SetIsEligible(true);
  web_state_->SetVisibleURL(GURL("chrome://newtab"));  // Ineligible URL
  EXPECT_FALSE([mediator_ geminiEntryPoint].enabled);
}

// Tests that lensEntryPointForTraitCollection reflects conditions correctly.
TEST_F(PageActionMenuMediatorTest, LensEntryPoint) {
  scoped_feature_list_.InitWithFeatures({kPageActionMenu}, {});

  UITraitCollection* portrait_traits = [UITraitCollection
      traitCollectionWithVerticalSizeClass:UIUserInterfaceSizeClassRegular];
  UITraitCollection* landscape_traits = [UITraitCollection
      traitCollectionWithVerticalSizeClass:UIUserInterfaceSizeClassCompact];

  // 1. Default state in tests: Google DSE, Lens allowed by policy.
  // Verify that Lens is available in portrait.
  pref_service_->SetInteger(
      lens::prefs::kLensOverlaySettings,
      static_cast<int>(lens::prefs::LensOverlaySettingsPolicyValue::kEnabled));

  EXPECT_TRUE(
      [mediator_ lensEntryPointForTraitCollection:portrait_traits].enabled);

  // 2. Test landscape mode unavailability.
  EXPECT_FALSE(
      [mediator_ lensEntryPointForTraitCollection:landscape_traits].enabled);

  // 3. Test policy unavailability.
  pref_service_->SetInteger(
      lens::prefs::kLensOverlaySettings,
      static_cast<int>(lens::prefs::LensOverlaySettingsPolicyValue::kDisabled));
  EXPECT_FALSE(
      [mediator_ lensEntryPointForTraitCollection:portrait_traits].enabled);

  // Reset policy.
  pref_service_->SetInteger(
      lens::prefs::kLensOverlaySettings,
      static_cast<int>(lens::prefs::LensOverlaySettingsPolicyValue::kEnabled));

  // 4. Test non-Google DSE unavailability.
  TemplateURLService* template_url_service =
      search_engines_test_environment_.template_url_service();
  template_url_service->Load();

  TemplateURLData non_google_provider_data;
  non_google_provider_data.SetURL("https://www.nongoogle.com/?q={searchTerms}");
  auto* non_google_provider = template_url_service->Add(
      std::make_unique<TemplateURL>(non_google_provider_data));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      non_google_provider);

  EXPECT_FALSE(
      [mediator_ lensEntryPointForTraitCollection:portrait_traits].enabled);
}

// Tests that Popup Blocker availability and action work correctly.
TEST_F(PageActionMenuMediatorTest, PopupBlocker) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kPageActionMenu, {}},
       {kProactiveSuggestionsFramework, {{"PopupBlocker", "true"}}}},
      {});

  // BlockedPopupTabHelper requires a real WebState to function properly (it
  // relies on WebStateImpl internals that FakeWebState doesn't mock).
  web::WebState::CreateParams params(browser_state_.get());
  std::unique_ptr<web::WebState> real_web_state = web::WebState::Create(params);

  BlockedPopupTabHelper::CreateForWebState(real_web_state.get());
  BlockedPopupTabHelper* helper =
      BlockedPopupTabHelper::FromWebState(real_web_state.get());
  ASSERT_TRUE(helper);

  BwgTabHelper::CreateForWebState(real_web_state.get());

  // We instantiate the mediator directly in the test scope rather than using a
  // helper method. This ensures ARC owns the object and releases it immediately
  // at the end of the scope, before `real_web_state` is destroyed. This
  // prevents dangling pointers in the mediator pointing to destroyed
  // TabHelpers.
  TemplateURLService* template_url_service =
      search_engines_test_environment_.template_url_service();
  PageActionMenuMediator* local_mediator = [[PageActionMenuMediator alloc]
            initWithWebState:real_web_state.get()
       authenticationService:auth_service_
          profilePrefService:pref_service_
          templateURLService:template_url_service
               geminiService:fake_gemini_service_.get()
             geminiTabHelper:BwgTabHelper::FromWebState(real_web_state.get())
         readerModeTabHelper:nil
      hostContentSettingsMap:settings_map_];

  FakePageActionMenuConsumer* local_consumer =
      [[FakePageActionMenuConsumer alloc] init];
  local_mediator.consumer = local_consumer;

  // 1. Default state: No popups blocked.
  EXPECT_FALSE([local_mediator isFeatureAvailable:PageActionMenuPopupBlocker]);
  EXPECT_EQ([local_mediator blockedPopupCount], 0U);

  // 2. Add a blocked popup.
  GURL source_url("https://example.com");

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURL(source_url),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::POPUPS,
      CONTENT_SETTING_BLOCK);

  GURL popup_url("https://popup.com");
  helper->HandlePopup(popup_url,
                      web::Referrer(source_url, web::ReferrerPolicyDefault));

  EXPECT_TRUE([local_mediator isFeatureAvailable:PageActionMenuPopupBlocker]);
  EXPECT_EQ([local_mediator blockedPopupCount], 1U);

  // 3. Test allowBlockedPopups action.
  [local_mediator allowBlockedPopups];

  // Verify content setting changed to ALLOW.
  ContentSetting setting = settings_map_->GetContentSetting(
      source_url, source_url, ContentSettingsType::POPUPS);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  [local_mediator disconnect];
}

// Tests that updatePermission updates WebState state correctly.
TEST_F(PageActionMenuMediatorTest, UpdatePermission) {
  scoped_feature_list_.InitWithFeatures({kPageActionMenu}, {});

  // 1. Test camera permission.
  [mediator_ updatePermission:YES forFeature:PageActionMenuCameraPermission];
  EXPECT_EQ(web_state_->GetStateForPermission(web::PermissionCamera),
            web::PermissionStateAllowed);

  [mediator_ updatePermission:NO forFeature:PageActionMenuCameraPermission];
  EXPECT_EQ(web_state_->GetStateForPermission(web::PermissionCamera),
            web::PermissionStateBlocked);

  // 2. Test microphone permission.
  [mediator_ updatePermission:YES
                   forFeature:PageActionMenuMicrophonePermission];
  EXPECT_EQ(web_state_->GetStateForPermission(web::PermissionMicrophone),
            web::PermissionStateAllowed);

  [mediator_ updatePermission:NO forFeature:PageActionMenuMicrophonePermission];
  EXPECT_EQ(web_state_->GetStateForPermission(web::PermissionMicrophone),
            web::PermissionStateBlocked);
}
