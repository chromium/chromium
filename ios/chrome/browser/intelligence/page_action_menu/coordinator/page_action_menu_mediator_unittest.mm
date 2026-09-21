// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/lens/lens_overlay_permission_utils.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/sync/test/test_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_consumer.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_content_entry_point.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_feature.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/web/model/blocked_popup_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Fake consumer that absorbs delegate events during testing.
@interface FakePageActionMenuConsumer : NSObject <PageActionMenuConsumer>
@property(nonatomic, assign) BOOL permissionStateChangedCalled;
@end
@implementation FakePageActionMenuConsumer
- (void)pageLoadStatusChanged {
}
- (void)permissionStateChanged {
  self.permissionStateChangedCalled = YES;
}
@end

namespace {

// Returns the camera permission feature among `features`, or nil if the camera
// row is not shown.
PageActionMenuFeature* CameraFeature(
    NSArray<PageActionMenuFeature*>* features) {
  for (PageActionMenuFeature* feature in features) {
    if (feature.featureType == PageActionMenuCameraPermission) {
      return feature;
    }
  }
  return nil;
}

}  // namespace

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
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        GeminiServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeGeminiService>();
            }));

    browser_state_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    // Fetch required services from the profile.
    auth_service_ = AuthenticationServiceFactory::GetForProfile(browser_state_);
    pref_service_ = browser_state_->GetPrefs();
    ASSERT_TRUE(pref_service_);
    fake_gemini_service_ = static_cast<FakeGeminiService*>(
        GeminiServiceFactory::GetForProfile(browser_state_));
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(browser_state_);
    web_state_->WasShown();

    // Set up search engines environment and content settings map.
    TemplateURLService* template_url_service =
        search_engines_test_environment_.template_url_service();
    settings_map_ =
        ios::HostContentSettingsMapFactory::GetForProfile(browser_state_);
    ASSERT_TRUE(settings_map_);

    // Attach required tab helpers to the fake web state.
    GeminiTabHelper::CreateForWebState(web_state_.get());
    gemini_tab_helper_ = GeminiTabHelper::FromWebState(web_state_.get());

    // Initialize the default mediator with the fake web state and services.
    mediator_ = [[PageActionMenuMediator alloc]
              initWithWebState:web_state_.get()
         authenticationService:auth_service_
            profilePrefService:pref_service_
            templateURLService:template_url_service
                 geminiService:fake_gemini_service_.get()
               geminiTabHelper:gemini_tab_helper_
           readerModeTabHelper:nil
        readerModeBrowserAgent:nil
        hostContentSettingsMap:settings_map_];
    fake_consumer_ = [[FakePageActionMenuConsumer alloc] init];
    mediator_.consumer = fake_consumer_;
  }
  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    fake_consumer_ = nil;
    gemini_tab_helper_ = nullptr;
    web_state_.reset();
    fake_gemini_service_ = nullptr;
    settings_map_ = nullptr;
    pref_service_ = nullptr;
    auth_service_ = nullptr;
    browser_state_ = nullptr;
    PlatformTest::TearDown();
  }

  void SignIn() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser_state_);
    signin::AccountAvailabilityOptionsBuilder options_builder;
    options_builder.AsPrimary(signin::ConsentLevel::kSignin);
    options_builder.WithGaiaId(identity.gaiaId);
    signin::MakeAccountAvailable(
        identity_manager,
        options_builder.Build(base::SysNSStringToUTF8(identity.userEmail)));

    auth_service_->SignIn(identity, signin_metrics::AccessPoint::kStartPage);
  }

  web::WebTaskEnvironment task_environment_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> browser_state_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<HostContentSettingsMap> settings_map_ = nullptr;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<FakeGeminiService> fake_gemini_service_ = nullptr;
  raw_ptr<GeminiTabHelper> gemini_tab_helper_;
  PageActionMenuMediator* mediator_;
  FakePageActionMenuConsumer* fake_consumer_;
};

// Tests that isGeminiAvailable accurately reflects underlying service and
// tab helper state.
TEST_F(PageActionMenuMediatorTest, IsGeminiAvailable) {
  // Enable PageActionMenu to simplify availability checks.
  scoped_feature_list_.InitWithFeatures({kPageActionMenu}, {});

  SignIn();

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

  GeminiTabHelper::CreateForWebState(real_web_state.get());

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
             geminiTabHelper:GeminiTabHelper::FromWebState(real_web_state.get())
         readerModeTabHelper:nil
      readerModeBrowserAgent:nil
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

// Tests that updatePermissionSetting updates both the session permission state
// and the persisted content setting of the site.
TEST_F(PageActionMenuMediatorTest, UpdatePermissionSetting) {
  scoped_feature_list_.InitWithFeatures(
      {kPageActionMenu, kDomainLevelSitePermissions}, {});

  const GURL url("https://example.com");
  web_state_->SetCurrentURL(url);

  // 1. Always allow persists an ALLOW content setting.
  [mediator_
      updatePermissionSetting:PageActionMenuPermissionSetting::kAlwaysAllow
                   forFeature:PageActionMenuCameraPermission];
  EXPECT_EQ(web_state_->GetStateForPermission(web::PermissionCamera),
            web::PermissionStateAllowed);
  EXPECT_EQ(settings_map_->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_CAMERA),
            CONTENT_SETTING_ALLOW);

  // 2. Never allow persists a BLOCK content setting and blocks the session.
  [mediator_
      updatePermissionSetting:PageActionMenuPermissionSetting::kNeverAllow
                   forFeature:PageActionMenuCameraPermission];
  EXPECT_EQ(web_state_->GetStateForPermission(web::PermissionCamera),
            web::PermissionStateBlocked);
  EXPECT_EQ(settings_map_->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_CAMERA),
            CONTENT_SETTING_BLOCK);

  // 3. Allow once clears the persisted decision but grants for the session.
  [mediator_ updatePermissionSetting:PageActionMenuPermissionSetting::kAllowOnce
                          forFeature:PageActionMenuCameraPermission];
  EXPECT_EQ(web_state_->GetStateForPermission(web::PermissionCamera),
            web::PermissionStateAllowed);
  EXPECT_EQ(settings_map_->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_CAMERA),
            CONTENT_SETTING_ASK);

  // 4. The microphone permission is updated independently.
  [mediator_
      updatePermissionSetting:PageActionMenuPermissionSetting::kAlwaysAllow
                   forFeature:PageActionMenuMicrophonePermission];
  EXPECT_EQ(settings_map_->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_MIC),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings_map_->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_CAMERA),
            CONTENT_SETTING_ASK);
}

// Tests that permission rows are shown as dropdowns reflecting the persisted
// content setting, and remain visible once the permission is blocked.
TEST_F(PageActionMenuMediatorTest, PermissionFeatureDropdown) {
  scoped_feature_list_.InitWithFeatures(
      {kPageActionMenu, kDomainLevelSitePermissions}, {});

  const GURL url("https://example.com");
  web_state_->SetCurrentURL(url);
  web_state_->SetStateForPermission(web::PermissionStateAllowed,
                                    web::PermissionCamera);

  // 1. Without a persisted decision, the access lasts for this visit only.
  PageActionMenuFeature* feature = CameraFeature([mediator_ activeFeatures]);
  ASSERT_TRUE(feature);
  EXPECT_EQ(feature.actionType, PageActionMenuDropdownAction);
  EXPECT_EQ(feature.permissionSetting,
            PageActionMenuPermissionSetting::kAllowOnce);

  // 2. A persisted ALLOW is surfaced as "always allow".
  settings_map_->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);
  feature = CameraFeature([mediator_ activeFeatures]);
  ASSERT_TRUE(feature);
  EXPECT_EQ(feature.permissionSetting,
            PageActionMenuPermissionSetting::kAlwaysAllow);

  // 3. If the session blocks a permission while persisted setting is ALLOW,
  // session state takes precedence and consumer is notified.
  fake_consumer_.permissionStateChangedCalled = NO;
  web_state_->SetStateForPermission(web::PermissionStateBlocked,
                                    web::PermissionCamera);
  EXPECT_TRUE(fake_consumer_.permissionStateChangedCalled);
  feature = CameraFeature([mediator_ activeFeatures]);
  ASSERT_TRUE(feature);
  EXPECT_EQ(feature.permissionSetting,
            PageActionMenuPermissionSetting::kNeverAllow);

  // 4. A persisted BLOCK is surfaced as "never allow" even when session state
  // is not blocked.
  web_state_->SetStateForPermission(web::PermissionStateAllowed,
                                    web::PermissionCamera);
  settings_map_->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_BLOCK);
  feature = CameraFeature([mediator_ activeFeatures]);
  ASSERT_TRUE(feature);
  EXPECT_EQ(feature.permissionSetting,
            PageActionMenuPermissionSetting::kNeverAllow);

  // 5. A blocked permission keeps its row so that it can be changed back.
  [mediator_
      updatePermissionSetting:PageActionMenuPermissionSetting::kNeverAllow
                   forFeature:PageActionMenuCameraPermission];
  EXPECT_TRUE([mediator_ isFeatureAvailable:PageActionMenuCameraPermission]);
  feature = CameraFeature([mediator_ activeFeatures]);
  ASSERT_TRUE(feature);
  EXPECT_EQ(feature.permissionSetting,
            PageActionMenuPermissionSetting::kNeverAllow);
}

// Tests that permission rows keep using a toggle, and are hidden when the
// permission is not allowed, while domain level site permissions are disabled.
TEST_F(PageActionMenuMediatorTest, PermissionFeatureToggle) {
  scoped_feature_list_.InitWithFeatures({kPageActionMenu},
                                        {kDomainLevelSitePermissions});

  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->SetStateForPermission(web::PermissionStateAllowed,
                                    web::PermissionCamera);

  PageActionMenuFeature* feature = CameraFeature([mediator_ activeFeatures]);
  ASSERT_TRUE(feature);
  EXPECT_EQ(feature.actionType, PageActionMenuToggleAction);
  EXPECT_TRUE(feature.toggleState);

  web_state_->SetStateForPermission(web::PermissionStateBlocked,
                                    web::PermissionCamera);
  EXPECT_FALSE([mediator_ isFeatureAvailable:PageActionMenuCameraPermission]);
  EXPECT_FALSE(CameraFeature([mediator_ activeFeatures]));
}
