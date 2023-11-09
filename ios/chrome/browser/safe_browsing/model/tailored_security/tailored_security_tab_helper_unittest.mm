// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_tab_helper.h"

#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_observer_util.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/chrome_tailored_security_service.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace safe_browsing {

namespace {

// Mock class for TailoredSecurityService.
class MockTailoredSecurityService : public ChromeTailoredSecurityService {
 public:
  MockTailoredSecurityService()
      : ChromeTailoredSecurityService(/*state=*/nullptr,
                                      /*identity_manager=*/nullptr,
                                      /*sync_service=*/nullptr) {}
  MockTailoredSecurityService(ChromeBrowserState* browser_state,
                              signin::IdentityManager* identity_manager)
      : ChromeTailoredSecurityService(browser_state,
                                      identity_manager,
                                      /*sync_service=*/nullptr) {}
  MOCK_METHOD0(RemoveQueryRequest, void());
  MOCK_METHOD2(MaybeNotifySyncUser, void(bool, base::Time));

  void CheckQueryRequest() {
    TailoredSecurityService::StartRequest(
        base::BindOnce(&MockTailoredSecurityService::FakeCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Methods created to simulate and validate behavior.
  void FakeCallback(bool is_enabled, base::Time previous_update) {}

  base::WeakPtrFactory<MockTailoredSecurityService> weak_ptr_factory_{this};
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
    mock_service_ = std::make_unique<MockTailoredSecurityService>(
        chrome_browser_state_.get(), GetIdentityManager());
    TailoredSecurityTabHelper::CreateForWebState(&web_state_,
                                                 mock_service_.get());
    tab_helper_ = TailoredSecurityTabHelper::FromWebState(&web_state_);
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForBrowserState(
        chrome_browser_state_.get());
  }

  size_t GetActiveQueryRequest() {
    return mock_service_.get()->active_query_request_;
  }

  bool IsSavedCallbackNull() {
    return mock_service_.get()->saved_callback_.is_null();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::FakeWebState web_state_;
  std::unique_ptr<MockTailoredSecurityService> mock_service_;
  TailoredSecurityTabHelper* tab_helper_;
};

// Tests if query request is added when a WebState is shown and removing the
// request when the WebState is hidden.
TEST_F(TailoredSecurityTabHelperTest, QueryRequestOnFocus) {
  PerformFakeNavigation("https://google.com", &web_state_);
  EXPECT_EQ(GetActiveQueryRequest(), static_cast<size_t>(1));

  EXPECT_CALL(*mock_service_.get(), RemoveQueryRequest());
  tab_helper_->WasHidden(nullptr);
}

// Tests how the tab helper responds to a mock navigation.
TEST_F(TailoredSecurityTabHelperTest, QueryRequestOnNavigation) {
  tab_helper_->WasShown(nullptr);

  PerformFakeNavigation("https://google.com", &web_state_);
  EXPECT_EQ(GetActiveQueryRequest(), static_cast<size_t>(1));

  EXPECT_CALL(*mock_service_.get(), RemoveQueryRequest());
  PerformFakeNavigation("https://example.com", &web_state_);
}

// Tests how the tab helper responds an observer call for a consented and
// enabled message prompt.
TEST_F(TailoredSecurityTabHelperTest,
       SyncNotificationForConsentedEnabledMessage) {
  InfoBarManagerImpl::CreateForWebState(&web_state_);
  web_state_.WasShown();

  // When a sync notification request is sent and the user is synced, the
  // SafeBrowsingState should automatically change to Enhanced Protection.
  tab_helper_->OnSyncNotificationMessageRequest(/*is_enabled=*/true);
  EXPECT_TRUE(
      safe_browsing::GetSafeBrowsingState(*chrome_browser_state_->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
}

// Tests how the tab helper responds an observer call for a consented and
// disabled message prompt.
TEST_F(TailoredSecurityTabHelperTest,
       SyncNotificationForConsentedDisabledMessage) {
  // When a sync notification request is sent and the user is synced, the
  // SafeBrowsingState should automatically change to Standard Protection.
  tab_helper_->OnSyncNotificationMessageRequest(/*is_enabled=*/false);
  EXPECT_TRUE(
      safe_browsing::GetSafeBrowsingState(*chrome_browser_state_->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
}

// Tests that method early returns if the WebState is hidden and doesn't change
// the SafeBrowsingState for sync notifications.
TEST_F(TailoredSecurityTabHelperTest, OnSyncNotificationRequestEarlyReturn) {
  web_state_.WasHidden();

  // When a sync notification request is sent and the user is synced, the
  // SafeBrowsingState should automatically change to Standard Protection.
  tab_helper_->OnSyncNotificationMessageRequest(/*is_enabled=*/true);
  EXPECT_TRUE(
      safe_browsing::GetSafeBrowsingState(*chrome_browser_state_->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
}

// Tests that an infobar is created when the tailored security bit is changed to
// true.
TEST_F(TailoredSecurityTabHelperTest,
       InfoBarCreatedOnTailoredSecurityBitChanged) {
  InfoBarManagerImpl::CreateForWebState(&web_state_);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(&web_state_);

  // When a nonsynced in-flow message prompt is triggered, the message prompt
  // should show for the WebState that is currently shown.
  web_state_.WasShown();
  tab_helper_->OnTailoredSecurityBitChanged(/*enabled=*/true,
                                            base::Time::Now());
  EXPECT_EQ(infobar_manager->infobars().size(), 1u);
  EXPECT_TRUE(chrome_browser_state_->GetPrefs()->GetBoolean(
      prefs::kAccountTailoredSecurityShownNotification));
}

// Tests that an infobar is not created when the WebState is hidden.
TEST_F(TailoredSecurityTabHelperTest, InfobarNotCreatedOnHiddenWebState) {
  InfoBarManagerImpl::CreateForWebState(&web_state_);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(&web_state_);

  // When a nonsynced in-flow message prompt is triggered, the message prompt
  // should not show for the WebState that is currently hidden.
  web_state_.WasHidden();
  tab_helper_->OnTailoredSecurityBitChanged(/*enabled=*/true,
                                            base::Time::Now());
  EXPECT_EQ(infobar_manager->infobars().size(), 0u);
  EXPECT_FALSE(chrome_browser_state_->GetPrefs()->GetBoolean(
      prefs::kAccountTailoredSecurityShownNotification));
}

// Tests that an infobar isn't created when the tailored security bit is changed
// to false.
TEST_F(TailoredSecurityTabHelperTest, EarlyReturnOnTailoredSecurityBitChanged) {
  InfoBarManagerImpl::CreateForWebState(&web_state_);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(&web_state_);

  tab_helper_->OnTailoredSecurityBitChanged(/*enabled=*/false,
                                            base::Time::Now());
  EXPECT_EQ(infobar_manager->infobars().size(), 0u);
  EXPECT_FALSE(chrome_browser_state_->GetPrefs()->GetBoolean(
      prefs::kAccountTailoredSecurityShownNotification));
}

// Tests that an infobar isn't created when the time difference is greater than
// kThresholdForInFlowNotification.
TEST_F(TailoredSecurityTabHelperTest,
       TailoredSecurityBitChangedAfterFiveMinutes) {
  InfoBarManagerImpl::CreateForWebState(&web_state_);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(&web_state_);

  tab_helper_->OnTailoredSecurityBitChanged(
      /*enabled=*/true,
      base::Time::Now() - (kThresholdForInFlowNotification + base::Minutes(1)));
  EXPECT_EQ(infobar_manager->infobars().size(), 0u);
  EXPECT_FALSE(chrome_browser_state_->GetPrefs()->GetBoolean(
      prefs::kAccountTailoredSecurityShownNotification));
}

// Test represents receiving a query request while the app is backgrounded and
// checking that no callback was sent.
TEST_F(TailoredSecurityTabHelperTest,
       TailoredSecurityWhenAppBackgroundedNoCallbackSent) {
  tab_helper_->WasShown(nullptr);

  // Represents app backgrounding and disabling querying.
  mock_service_->SetCanQuery(false);
  mock_service_->CheckQueryRequest();
  EXPECT_FALSE(IsSavedCallbackNull());
  PerformFakeNavigation("https://google.com", &web_state_);
  EXPECT_EQ(GetActiveQueryRequest(), static_cast<size_t>(0));
}

// Test represents receiving a query request while the app is backgrounded,
// storing the callback, and then calling it when the app is re-foregrounds.
TEST_F(TailoredSecurityTabHelperTest,
       TailoredSecuritySettingCanQueryTrueSendsPendingQuery) {
  tab_helper_->WasShown(nullptr);

  // Represents app not backgrounded and working normally.
  mock_service_->CheckQueryRequest();
  EXPECT_TRUE(IsSavedCallbackNull());

  // Represents app backgrounded, receiving a background request, not querying.
  mock_service_->SetCanQuery(false);
  mock_service_->CheckQueryRequest();
  EXPECT_FALSE(IsSavedCallbackNull());
  PerformFakeNavigation("https://google.com", &web_state_);
  EXPECT_EQ(GetActiveQueryRequest(), static_cast<size_t>(0));

  // Represents app foregrounded and using stored callback.
  mock_service_->SetCanQuery(true);
  EXPECT_TRUE(IsSavedCallbackNull());
}

}  // namespace safe_browsing
