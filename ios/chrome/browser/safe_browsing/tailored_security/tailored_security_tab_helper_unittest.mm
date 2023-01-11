// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/tailored_security/tailored_security_tab_helper.h"

#import "base/test/task_environment.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_observer_util.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace safe_browsing {

namespace {

// Mock class for TailoredSecurityService.
class MockTailoredSecurityService : public TailoredSecurityService {
 public:
  MockTailoredSecurityService() : TailoredSecurityService(nullptr, nullptr) {}
  MOCK_METHOD0(AddQueryRequest, void());
  MOCK_METHOD0(RemoveQueryRequest, void());
  MOCK_METHOD2(MaybeNotifySyncUser, void(bool, base::Time));
  MOCK_METHOD0(GetURLLoaderFactory,
               scoped_refptr<network::SharedURLLoaderFactory>());
};

// Starts and finishes a mock navigation successfully.
void PerformFakeNavigation(std::string url, web::FakeWebState* web_state) {
  web::FakeNavigationContext context;
  context.SetUrl(GURL(url));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state->OnNavigationStarted(&context);
  web_state->OnNavigationFinished(&context);
}

}  // namespace

class TailoredSecurityTabHelperTest : public PlatformTest {
 protected:
  TailoredSecurityTabHelperTest() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry =
        base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
    RegisterBrowserStatePrefs(registry.get());
    sync_preferences::PrefServiceMockFactory factory;

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(factory.CreateSyncable(registry.get()));
    chrome_browser_state_ = test_cbs_builder.Build();
    web_state_.SetBrowserState(chrome_browser_state_.get());
    // Needed to create InfoBarManager.
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::FakeWebState web_state_;
};

// Tests if query request is added when a WebState is shown and removing the
// request when the WebState is hidden.
TEST_F(TailoredSecurityTabHelperTest, QueryRequestOnFocus) {
  MockTailoredSecurityService mock_service;
  TailoredSecurityTabHelper::CreateForWebState(&web_state_, &mock_service);
  TailoredSecurityTabHelper* tab_helper =
      TailoredSecurityTabHelper::FromWebState(&web_state_);

  EXPECT_CALL(mock_service, AddQueryRequest());
  PerformFakeNavigation("https://google.com", &web_state_);

  EXPECT_CALL(mock_service, RemoveQueryRequest());
  tab_helper->WasHidden(nullptr);
}

// Tests how the tab helper responds to a mock navigation.
TEST_F(TailoredSecurityTabHelperTest, QueryRequestOnNavigation) {
  MockTailoredSecurityService mock_service;
  TailoredSecurityTabHelper::CreateForWebState(&web_state_, &mock_service);

  TailoredSecurityTabHelper* tab_helper =
      TailoredSecurityTabHelper::FromWebState(&web_state_);

  tab_helper->WasShown(nullptr);

  EXPECT_CALL(mock_service, AddQueryRequest());
  PerformFakeNavigation("https://google.com", &web_state_);

  EXPECT_CALL(mock_service, RemoveQueryRequest());
  PerformFakeNavigation("https://example.com", &web_state_);
}

// Tests how the tab helper responds an observer call for a consented and
// enabled message prompt.
TEST_F(TailoredSecurityTabHelperTest,
       SyncNotificationForConsentedEnabledMessage) {
  MockTailoredSecurityService mock_service;
  TailoredSecurityTabHelper::CreateForWebState(&web_state_, &mock_service);
  InfoBarManagerImpl::CreateForWebState(&web_state_);
  TailoredSecurityTabHelper* tab_helper =
      TailoredSecurityTabHelper::FromWebState(&web_state_);
  web_state_.WasShown();

  // When a sync notification request is sent and the user is synced, the
  // SafeBrowsingState should automatically change to Enhanced Protection.
  tab_helper->OnSyncNotificationMessageRequest(/*is_enabled=*/true);
  EXPECT_TRUE(
      safe_browsing::GetSafeBrowsingState(*chrome_browser_state_->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
}

// Tests how the tab helper responds an observer call for a consented and
// disabled message prompt.
TEST_F(TailoredSecurityTabHelperTest,
       SyncNotificationForConsentedDisabledMessage) {
  MockTailoredSecurityService mock_service;
  TailoredSecurityTabHelper::CreateForWebState(&web_state_, &mock_service);
  InfoBarManagerImpl::CreateForWebState(&web_state_);
  TailoredSecurityTabHelper* tab_helper =
      TailoredSecurityTabHelper::FromWebState(&web_state_);

  // When a sync notification request is sent and the user is synced, the
  // SafeBrowsingState should automatically change to Standard Protection.
  tab_helper->OnSyncNotificationMessageRequest(/*is_enabled=*/false);
  EXPECT_TRUE(
      safe_browsing::GetSafeBrowsingState(*chrome_browser_state_->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
}

// Tests that method early returns if the WebState is hidden and doesn't change
// the SafeBrowsingState for sync notifications.
TEST_F(TailoredSecurityTabHelperTest, OnSyncNotificationRequestEarlyReturn) {
  MockTailoredSecurityService mock_service;
  TailoredSecurityTabHelper::CreateForWebState(&web_state_, &mock_service);
  TailoredSecurityTabHelper* tab_helper =
      TailoredSecurityTabHelper::FromWebState(&web_state_);
  web_state_.WasHidden();

  // When a sync notification request is sent and the user is synced, the
  // SafeBrowsingState should automatically change to Standard Protection.
  tab_helper->OnSyncNotificationMessageRequest(/*is_enabled=*/true);
  EXPECT_TRUE(
      safe_browsing::GetSafeBrowsingState(*chrome_browser_state_->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
}

// Tests that an infobar is created when the tailored security bit is changed to
// true.
TEST_F(TailoredSecurityTabHelperTest,
       InfoBarCreatedOnTailoredSecurityBitChanged) {
  MockTailoredSecurityService mock_service;
  TailoredSecurityTabHelper::CreateForWebState(&web_state_, &mock_service);
  InfoBarManagerImpl::CreateForWebState(&web_state_);
  TailoredSecurityTabHelper* tab_helper =
      TailoredSecurityTabHelper::FromWebState(&web_state_);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(&web_state_);

  // When a nonsynced in-flow message prompt is triggered, the message prompt
  // should show for the WebState that is currently shown.
  web_state_.WasShown();
  tab_helper->OnTailoredSecurityBitChanged(/*enabled=*/true, base::Time::Now());
  EXPECT_TRUE(infobar_manager->infobar_count() == 1);
  EXPECT_TRUE(chrome_browser_state_->GetPrefs()->GetBoolean(
      prefs::kAccountTailoredSecurityShownNotification));
}

// Tests that an infobar is not created when the WebState is hidden.
TEST_F(TailoredSecurityTabHelperTest, InfobarNotCreatedOnHiddenWebState) {
  MockTailoredSecurityService mock_service;
  TailoredSecurityTabHelper::CreateForWebState(&web_state_, &mock_service);
  InfoBarManagerImpl::CreateForWebState(&web_state_);
  TailoredSecurityTabHelper* tab_helper =
      TailoredSecurityTabHelper::FromWebState(&web_state_);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(&web_state_);

  // When a nonsynced in-flow message prompt is triggered, the message prompt
  // should not show for the WebState that is currently hidden.
  web_state_.WasHidden();
  tab_helper->OnTailoredSecurityBitChanged(/*enabled=*/true, base::Time::Now());
  EXPECT_TRUE(infobar_manager->infobar_count() == 0);
  EXPECT_FALSE(chrome_browser_state_->GetPrefs()->GetBoolean(
      prefs::kAccountTailoredSecurityShownNotification));
}

// Tests that an infobar isn't created when the tailored security bit is changed
// to false.
TEST_F(TailoredSecurityTabHelperTest, EarlyReturnOnTailoredSecurityBitChanged) {
  MockTailoredSecurityService mock_service;
  TailoredSecurityTabHelper::CreateForWebState(&web_state_, &mock_service);
  InfoBarManagerImpl::CreateForWebState(&web_state_);
  TailoredSecurityTabHelper* tab_helper =
      TailoredSecurityTabHelper::FromWebState(&web_state_);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(&web_state_);

  tab_helper->OnTailoredSecurityBitChanged(/*enabled=*/false,
                                           base::Time::Now());
  EXPECT_TRUE(infobar_manager->infobar_count() == 0);
  EXPECT_FALSE(chrome_browser_state_->GetPrefs()->GetBoolean(
      prefs::kAccountTailoredSecurityShownNotification));
}

// Tests that an infobar isn't created when the time difference is greater than
// kThresholdForInFlowNotification.
TEST_F(TailoredSecurityTabHelperTest,
       TailoredSecurityBitChangedAfterFiveMinutes) {
  MockTailoredSecurityService mock_service;
  TailoredSecurityTabHelper::CreateForWebState(&web_state_, &mock_service);
  InfoBarManagerImpl::CreateForWebState(&web_state_);
  TailoredSecurityTabHelper* tab_helper =
      TailoredSecurityTabHelper::FromWebState(&web_state_);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(&web_state_);

  tab_helper->OnTailoredSecurityBitChanged(
      /*enabled=*/true,
      base::Time::Now() - (kThresholdForInFlowNotification + base::Minutes(1)));
  EXPECT_TRUE(infobar_manager->infobar_count() == 0);
  EXPECT_FALSE(chrome_browser_state_->GetPrefs()->GetBoolean(
      prefs::kAccountTailoredSecurityShownNotification));
}

}  // namespace safe_browsing
