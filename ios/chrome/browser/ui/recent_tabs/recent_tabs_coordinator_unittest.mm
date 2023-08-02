// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/bind.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/primary_account_mutator.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/fake_model_type_controller_delegate.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_user_events/global_id_mapper.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/recent_tabs/sessions_sync_user_state.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

using testing::_;
using testing::Return;

namespace {

class SessionSyncServiceMockForRecentTabsTableCoordinator
    : public sync_sessions::SessionSyncService {
 public:
  SessionSyncServiceMockForRecentTabsTableCoordinator() {}
  ~SessionSyncServiceMockForRecentTabsTableCoordinator() override {}

  MOCK_CONST_METHOD0(GetGlobalIdMapper, syncer::GlobalIdMapper*());
  MOCK_METHOD0(GetOpenTabsUIDelegate, sync_sessions::OpenTabsUIDelegate*());
  MOCK_METHOD1(
      SubscribeToForeignSessionsChanged,
      base::CallbackListSubscription(const base::RepeatingClosure& cb));
  MOCK_METHOD0(ScheduleGarbageCollection, void());
  MOCK_METHOD0(GetControllerDelegate,
               base::WeakPtr<syncer::ModelTypeControllerDelegate>());
  MOCK_METHOD1(ProxyTabsStateChanged,
               void(syncer::DataTypeController::State state));
};

std::unique_ptr<KeyedService>
BuildMockSessionSyncServiceForRecentTabsTableCoordinator(
    web::BrowserState* context) {
  return std::make_unique<
      testing::NiceMock<SessionSyncServiceMockForRecentTabsTableCoordinator>>();
}

class OpenTabsUIDelegateMock : public sync_sessions::OpenTabsUIDelegate {
 public:
  OpenTabsUIDelegateMock() {}
  ~OpenTabsUIDelegateMock() override {}

  MOCK_METHOD1(
      GetAllForeignSessions,
      bool(std::vector<const sync_sessions::SyncedSession*>* sessions));
  MOCK_METHOD3(GetForeignTab,
               bool(const std::string& tag,
                    const SessionID tab_id,
                    const sessions::SessionTab** tab));
  MOCK_METHOD1(DeleteForeignSession, void(const std::string& tag));
  MOCK_METHOD2(GetForeignSession,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionWindow*>* windows));
  MOCK_METHOD2(GetForeignSessionTabs,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionTab*>* tabs));
  MOCK_METHOD1(GetLocalSession,
               bool(const sync_sessions::SyncedSession** local));
};

class GlobalIdMapperMock : public syncer::GlobalIdMapper {
 public:
  GlobalIdMapperMock() {}
  ~GlobalIdMapperMock() {}

  MOCK_METHOD1(AddGlobalIdChangeObserver,
               void(syncer::GlobalIdChange callback));
  MOCK_METHOD1(GetLatestGlobalId, int64_t(int64_t global_id));
};

class RecentTabsTableCoordinatorTest : public BlockCleanupTest {
 public:
  RecentTabsTableCoordinatorTest()
      : no_error_(GoogleServiceAuthError::NONE),
        fake_controller_delegate_(syncer::SESSIONS) {}

 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    test_cbs_builder.AddTestingFactory(
        SessionSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            &BuildMockSessionSyncServiceForRecentTabsTableCoordinator));
    test_cbs_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;

    BlockCleanupTest::TearDown();
  }

  void SetupSyncState(BOOL signedIn,
                      BOOL syncEnabled,
                      BOOL syncCompleted,
                      BOOL hasForeignSessions) {
    // TODO(crbug.com/1466884): Delete the usage of ConsentLevel::kSync after
    // Phase 2 on iOS is launched. See ConsentLevel::kSync documentation for
    // details.
    if (signedIn) {
      identity_test_env_.MakePrimaryAccountAvailable(
          "test@test.com", signin::ConsentLevel::kSync);
    } else if (identity_test_env_.identity_manager()->HasPrimaryAccount(
                   signin::ConsentLevel::kSync)) {
      auto* account_mutator =
          identity_test_env_.identity_manager()->GetPrimaryAccountMutator();

      // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
      DCHECK(account_mutator);
      account_mutator->ClearPrimaryAccount(
          signin_metrics::ProfileSignout::kTest,
          signin_metrics::SignoutDelete::kIgnoreMetric);
    }

    SessionSyncServiceMockForRecentTabsTableCoordinator* session_sync_service =
        static_cast<SessionSyncServiceMockForRecentTabsTableCoordinator*>(
            SessionSyncServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));

    // Needed by SyncService's initialization, triggered during initialization
    // of SyncSetupServiceMock.
    ON_CALL(*session_sync_service, GetControllerDelegate())
        .WillByDefault(Return(fake_controller_delegate_.GetWeakPtr()));
    ON_CALL(*session_sync_service, GetGlobalIdMapper())
        .WillByDefault(Return(&global_id_mapper_));

    SyncSetupServiceMock* syncSetupService = static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
    ON_CALL(*syncSetupService, IsSyncFeatureEnabled())
        .WillByDefault(Return(syncEnabled));
    ON_CALL(*syncSetupService,
            IsDataTypePreferred(syncer::UserSelectableType::kTabs))
        .WillByDefault(Return(true));

    if (syncCompleted) {
      ON_CALL(*session_sync_service, GetOpenTabsUIDelegate())
          .WillByDefault(Return(&open_tabs_ui_delegate_));
      ON_CALL(open_tabs_ui_delegate_, GetAllForeignSessions(_))
          .WillByDefault(Return(hasForeignSessions));
    }
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
  }

  void CreateController() {
    coordinator_ = [[RecentTabsCoordinator alloc]
        initWithBaseViewController:nil
                           browser:browser_.get()];
    mock_application_commands_handler_ =
        [OCMockObject mockForProtocol:@protocol(ApplicationCommands)];
    mock_application_settings_commands_handler_ =
        [OCMockObject mockForProtocol:@protocol(ApplicationSettingsCommands)];
    mock_browsing_data_commands_handler_ =
        [OCMockObject mockForProtocol:@protocol(BrowsingDataCommands)];

    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_settings_commands_handler_
                     forProtocol:@protocol(ApplicationSettingsCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_browsing_data_commands_handler_
                     forProtocol:@protocol(BrowsingDataCommands)];

    [coordinator_ start];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  GoogleServiceAuthError no_error_;
  IOSChromeScopedTestingLocalState local_state_;
  signin::IdentityTestEnvironment identity_test_env_;

  syncer::FakeModelTypeControllerDelegate fake_controller_delegate_;
  testing::NiceMock<OpenTabsUIDelegateMock> open_tabs_ui_delegate_;
  testing::NiceMock<GlobalIdMapperMock> global_id_mapper_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;

  // Must be declared *after* `chrome_browser_state_` so it can outlive it.
  RecentTabsCoordinator* coordinator_;
  id<ApplicationCommands> mock_application_commands_handler_;
  id<ApplicationSettingsCommands> mock_application_settings_commands_handler_;
  id<BrowsingDataCommands> mock_browsing_data_commands_handler_;
};

TEST_F(RecentTabsTableCoordinatorTest, TestConstructorDestructor) {
  SetupSyncState(NO, NO, NO, NO);
  CreateController();
  EXPECT_TRUE(coordinator_);
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedOut) {
  // TODO(crbug.com/907495): Actual test expectations are missing below.
  SetupSyncState(NO, NO, NO, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncOff) {
  // TODO(crbug.com/907495): Actual test expectations are missing below.
  SetupSyncState(YES, NO, NO, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncInProgress) {
  // TODO(crbug.com/907495): Actual test expectations are missing below.
  SetupSyncState(YES, YES, NO, NO);
  CreateController();
}
TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncOnWithoutSessions) {
  // TODO(crbug.com/907495): Actual test expectations are missing below.
  SetupSyncState(YES, YES, YES, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncOnWithSessions) {
  // TODO(crbug.com/907495): Actual test expectations are missing below.
  SetupSyncState(YES, YES, YES, YES);
  CreateController();
}

}  // namespace
