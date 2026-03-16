// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/ui_bundled/safe_browsing_coordinator.h"

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/infobars/core/infobar.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/chrome_tailored_security_service.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace safe_browsing {

namespace {

class MockTailoredSecurityService : public ChromeTailoredSecurityService {
 public:
  MockTailoredSecurityService(ProfileIOS* profile,
                              signin::IdentityManager* identity_manager)
      : ChromeTailoredSecurityService(profile,
                                      identity_manager,
                                      /*sync_service=*/nullptr) {}
};

std::unique_ptr<KeyedService> BuildMockTailoredSecurityService(
    ProfileIOS* profile) {
  return std::make_unique<MockTailoredSecurityService>(
      profile, IdentityManagerFactory::GetForProfile(profile));
}

// Starts and finishes a mock navigation successfully.
void PerformFakeNavigation(const GURL& url, web::FakeWebState* web_state) {
  web::FakeNavigationContext context;
  context.SetUrl(url);
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state->OnNavigationStarted(&context);
  web_state->OnNavigationFinished(&context);
}

}  // namespace

class SafeBrowsingCoordinatorTest : public PlatformTest {
 protected:
  SafeBrowsingCoordinatorTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        TailoredSecurityServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockTailoredSecurityService));
    profile_ = std::move(test_profile_builder).Build();

    client_ = std::make_unique<FakeSafeBrowsingClient>(profile_->GetPrefs());

    browser_ = std::make_unique<TestBrowser>(profile_.get());

    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_handler_
                     forProtocol:@protocol(SettingsCommands)];

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state_ = web_state.get();

    InfoBarManagerImpl::CreateForWebState(web_state_);

    // Create SafeBrowsingQueryManager.
    SafeBrowsingQueryManager::CreateForWebState(web_state_, client_.get());

    // Create SafeBrowsingTabHelper.
    SafeBrowsingTabHelper::CreateForWebState(web_state_, client_.get());

    browser_->GetWebStateList()->InsertWebState(std::move(web_state));
    browser_->GetWebStateList()->ActivateWebStateAt(0);

    coordinator_ = [[SafeBrowsingCoordinator alloc]
        initWithBaseViewController:nil
                           browser:browser_.get()];
    [coordinator_ start];
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    web_state_ = nullptr;
    browser_.reset();
    client_.reset();
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<FakeSafeBrowsingClient> client_;
  SafeBrowsingCoordinator* coordinator_;
  id mock_settings_handler_;
};

TEST_F(SafeBrowsingCoordinatorTest,
       SuppressInfobarWhenHandlingSyncNotification) {
  // Navigation is needed for InfoBarManager to have an active entry.
  PerformFakeNavigation(GURL("https://google.com"), web_state_);
  web_state_->WasShown();

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  EXPECT_EQ(infobar_manager->infobars().size(), 0u);

  MockTailoredSecurityService* service =
      static_cast<MockTailoredSecurityService*>(
          TailoredSecurityServiceFactory::GetForProfile(profile_.get()));

  // Simulate handling sync notification.
  {
    TailoredSecurityService::ScopedSyncNotificationGuard guard(*service);
    // Trigger a preference change that would normally show an infobar.
    profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
    EXPECT_EQ(infobar_manager->infobars().size(), 0u);
  }

  // After sync is done, changes should trigger infobar.
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  EXPECT_EQ(infobar_manager->infobars().size(), 1u);
}

TEST_F(SafeBrowsingCoordinatorTest,
       ShowInfobarWhenNotHandlingSyncNotification) {
  // Navigation is needed for InfoBarManager to have an active entry.
  PerformFakeNavigation(GURL("https://google.com"), web_state_);
  web_state_->WasShown();

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  EXPECT_EQ(infobar_manager->infobars().size(), 0u);

  // Trigger a preference change without handling sync notification.
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_EQ(infobar_manager->infobars().size(), 1u);
}

TEST_F(SafeBrowsingCoordinatorTest,
       ShowOnlyOneInfobarWhenEnabledViaTailoredSecurity) {
  // Navigation is needed for InfoBarManager to have an active entry.
  PerformFakeNavigation(GURL("https://google.com"), web_state_);
  web_state_->WasShown();

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  EXPECT_EQ(infobar_manager->infobars().size(), 0u);

  MockTailoredSecurityService* service =
      static_cast<MockTailoredSecurityService*>(
          TailoredSecurityServiceFactory::GetForProfile(profile_.get()));

  TailoredSecurityTabHelper::CreateForWebState(web_state_, service);
  TailoredSecurityTabHelper* tab_helper =
      TailoredSecurityTabHelper::FromWebState(web_state_);

  // Trigger the Tailored Security sync notification.
  // This will internaly set the preference AND show its own infobar.
  tab_helper->OnSyncNotificationMessageRequest(/*is_enabled=*/true);

  // Verify that only one infobar is shown (the one from
  // TailoredSecurityTabHelper). SafeBrowsingCoordinator should have suppressed
  // its own.
  ASSERT_EQ(infobar_manager->infobars().size(), 1u);
  InfoBarIOS* infobar =
      static_cast<InfoBarIOS*>(infobar_manager->infobars()[0]);
  EXPECT_EQ(infobar->infobar_type(),
            InfobarType::kInfobarTypeTailoredSecurityService);
}

}  // namespace safe_browsing
