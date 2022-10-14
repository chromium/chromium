// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/tailored_security/tailored_security_infobar_banner_interaction_handler.h"

#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/tailored_security_service_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_client_factory.h"
#import "ios/chrome/browser/safe_browsing/tailored_security/test/mock_tailored_security_service_infobar_delegate.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for TailoredSecurityInfobarBannerInteractionHandlerTest.
class TailoredSecurityInfobarBannerInteractionHandlerTest
    : public PlatformTest {
 public:
  TailoredSecurityInfobarBannerInteractionHandlerTest()
      : task_environment_(web::WebTaskEnvironment::IO_MAINLOOP),
        handler_(
            tailored_security_service_infobar_overlays::
                TailoredSecurityServiceBannerRequestConfig::RequestSupport()) {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry =
        base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
    RegisterBrowserStatePrefs(registry.get());
    sync_preferences::PrefServiceMockFactory factory;

    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &DefaultInfobarOverlayRequestFactory);
    InfoBarManagerImpl::CreateForWebState(&web_state_);

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(factory.CreateSyncable(registry.get()));
    chrome_browser_state_ = test_cbs_builder.Build();
    web_state_.SetBrowserState(chrome_browser_state_.get());

    SafeBrowsingClient* client = SafeBrowsingClientFactory::GetForBrowserState(
        chrome_browser_state_.get());
    SafeBrowsingQueryManager::CreateForWebState(&web_state_, client);
    SafeBrowsingTabHelper::CreateForWebState(&web_state_, client);
  }

  safe_browsing::MockTailoredSecurityServiceInfobarDelegate& mock_delegate() {
    return *static_cast<
        safe_browsing::MockTailoredSecurityServiceInfobarDelegate*>(
        infobar_->delegate());
  }

  // Creates an infobar with a specific TailoredSecurityServiceMessageState.
  void CreateInfobarWithMessageState(
      safe_browsing::TailoredSecurityServiceMessageState message_state) {
    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeTailoredSecurityService,
        safe_browsing::MockTailoredSecurityServiceInfobarDelegate::Create(
            message_state, &web_state_));
    infobar_ = infobar.get();
    InfoBarManagerImpl::FromWebState(&web_state_)
        ->AddInfoBar(std::move(infobar));
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  TailoredSecurityInfobarBannerInteractionHandler handler_;
  web::FakeWebState web_state_;
  InfoBarIOS* infobar_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests MainButtonTapped() calls Accept() on the mock delegate and resets
// the infobar to be accepted for consented message prompt.
TEST_F(TailoredSecurityInfobarBannerInteractionHandlerTest,
       ConsentedMessagePromptButtonTapped) {
  CreateInfobarWithMessageState(
      safe_browsing::TailoredSecurityServiceMessageState::
          kConsentedAndFlowEnabled);
  ASSERT_FALSE(infobar_->accepted());
  handler_.MainButtonTapped(infobar_);
  EXPECT_TRUE(infobar_->accepted());
}

// Tests MainButtonTapped() calls Accept() on the mock delegate and resets
// the infobar to be accepted for unconsented message prompt.
TEST_F(TailoredSecurityInfobarBannerInteractionHandlerTest,
       UnconsentedMessagePromptButtonTapped) {
  CreateInfobarWithMessageState(
      safe_browsing::TailoredSecurityServiceMessageState::
          kUnconsentedAndFlowEnabled);
  ASSERT_FALSE(infobar_->accepted());
  handler_.MainButtonTapped(infobar_);
  EXPECT_TRUE(infobar_->accepted());
  EXPECT_TRUE(
      safe_browsing::GetSafeBrowsingState(*chrome_browser_state_->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
}
