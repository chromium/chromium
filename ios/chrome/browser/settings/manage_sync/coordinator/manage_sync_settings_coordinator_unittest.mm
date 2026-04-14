// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/manage_sync/coordinator/manage_sync_settings_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/authentication/signin/reauth/coordinator/signin_reauth_coordinator.h"
#import "ios/chrome/browser/settings/manage_sync/public/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface ManageSyncSettingsCoordinator (Testing) <
    SyncErrorSettingsCommandHandler>
- (void)showManageYourGoogleAccount;
@end

class ManageSyncSettingsCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::MockSyncService>();
            }));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    FakeSystemIdentityManager* fake_system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    fake_system_identity_manager->AddIdentity(
        [FakeSystemIdentity fakeIdentity1]);
    AuthenticationServiceFactory::GetForProfile(profile_.get())
        ->SignIn([FakeSystemIdentity fakeIdentity1],
                 signin_metrics::AccessPoint::kStartPage);

    browser_ = std::make_unique<TestBrowser>(profile_.get());

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());

    mock_sync_service_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));

    scene_handler_mock_ = OCMStrictProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:scene_handler_mock_
                     forProtocol:@protocol(SceneCommands)];

    base_navigation_controller_ = [[UINavigationController alloc] init];
    coordinator_ = [[ManageSyncSettingsCoordinator alloc]
        initWithBaseNavigationController:base_navigation_controller_
                                 browser:browser_.get()];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<syncer::MockSyncService> mock_sync_service_;
  id<SceneCommands> scene_handler_mock_;
  UINavigationController* base_navigation_controller_;
  ManageSyncSettingsCoordinator* coordinator_;
};

// Tests that openBookmarksLimitExceededHelp acknowledges the error and opens
// the help center article.
TEST_F(ManageSyncSettingsCoordinatorTest, TestOpenBookmarksLimitExceededHelp) {
  EXPECT_CALL(*mock_sync_service_,
              AcknowledgeBookmarksLimitExceededError(
                  syncer::SyncService::BookmarksLimitExceededHelpClickedSource::
                      kSettings));

  OCMExpect([scene_handler_mock_ closePresentedViewsAndOpenURL:[OCMArg any]]);

  [coordinator_ openBookmarksLimitExceededHelp];

  EXPECT_OCMOCK_VERIFY((OCMockObject*)scene_handler_mock_);
}

// Tests that showManageYourGoogleAccount opens a reauth dialog when the
// primary identity does not have valid auth.
TEST_F(ManageSyncSettingsCoordinatorTest,
       TestShowManageYourGoogleAccountInvalidAuth) {
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->SetPersistentAuthErrorForAccount(
          CoreAccountId::FromGaiaId([FakeSystemIdentity fakeIdentity1].gaiaId));

  id reauthMock = OCMClassMock([SigninReauthCoordinator class]);
  OCMExpect([reauthMock alloc]).andReturn(reauthMock);

  [coordinator_ showManageYourGoogleAccount];

  EXPECT_OCMOCK_VERIFY(reauthMock);
}
