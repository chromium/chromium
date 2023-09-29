// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "components/sessions/core/serialized_navigation_entry_test_helper.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/primary_account_mutator.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/fake_model_type_controller_delegate.h"
#import "components/sync/test/test_sync_service.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_sessions/synced_session.h"
#import "components/sync_user_events/global_id_mapper.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_setup_service.h"
#import "ios/chrome/browser/sync/model/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/recent_tabs/sessions_sync_user_state.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

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

// Returns a TestSyncService.
std::unique_ptr<KeyedService> BuildFakeSyncServiceFactory(
    web::BrowserState* context) {
  return std::make_unique<syncer::TestSyncService>();
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
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());

    test_cbs_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeSyncServiceFactory));

    test_cbs_builder.AddTestingFactory(
        SessionSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            &BuildMockSessionSyncServiceForRecentTabsTableCoordinator));
    test_cbs_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;

    BlockCleanupTest::TearDown();
  }

  void SetupSyncState(BOOL signed_in,
                      BOOL sync_enabled,
                      BOOL sync_completed,
                      BOOL has_foreign_sessions) {
    SessionSyncServiceMockForRecentTabsTableCoordinator* session_sync_service =
        static_cast<SessionSyncServiceMockForRecentTabsTableCoordinator*>(
            SessionSyncServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForBrowserState(chrome_browser_state_.get()));
    sync_service_->SetSetupInProgress(!sync_enabled);
    sync_service_->SetHasSyncConsent(sync_completed);

    // Needed by SyncService's initialization, triggered during initialization
    // of SyncSetupServiceMock.
    ON_CALL(*session_sync_service, GetControllerDelegate())
        .WillByDefault(Return(fake_controller_delegate_.GetWeakPtr()));
    ON_CALL(*session_sync_service, GetGlobalIdMapper())
        .WillByDefault(Return(&global_id_mapper_));

    if (signed_in) {
      AuthenticationService* authentication_service =
          AuthenticationServiceFactory::GetForBrowserState(
              chrome_browser_state_.get());

      FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];

      // Register a fake identity and set the expected capabilities.
      FakeSystemIdentityManager* system_identity_manager =
          FakeSystemIdentityManager::FromSystemIdentityManager(
              GetApplicationContext()->GetSystemIdentityManager());
      system_identity_manager->AddIdentity(identity);

      authentication_service->SignIn(
          identity, signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR);
      authentication_service->GrantSyncConsent(
          identity, signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR);
    }

    ON_CALL(*session_sync_service, GetOpenTabsUIDelegate())
        .WillByDefault(Return(&open_tabs_ui_delegate_));
    if (has_foreign_sessions) {
      sessions_.push_back(&sync_session_);

      open_tab_.navigations.push_back(
          sessions::SerializedNavigationEntryTestHelper::
              CreateNavigationForTest());
      open_tabs_.push_back(&open_tab_);

      ON_CALL(open_tabs_ui_delegate_, GetForeignSessionTabs(_, _))
          .WillByDefault(DoAll(SetArgPointee<1>(open_tabs_), Return(true)));
    }
    ON_CALL(open_tabs_ui_delegate_, GetAllForeignSessions(_))
        .WillByDefault(
            DoAll(SetArgPointee<0>(sessions_), Return(has_foreign_sessions)));
  }

  void CreateController() {
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    coordinator_ = [[RecentTabsCoordinator alloc]
        initWithBaseViewController:base_view_controller_
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

    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::Milliseconds(100), ^bool {
          return base_view_controller_.presentedViewController != nil;
        }));
    base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(100));
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  GoogleServiceAuthError no_error_;
  IOSChromeScopedTestingLocalState local_state_;

  syncer::FakeModelTypeControllerDelegate fake_controller_delegate_;
  testing::NiceMock<OpenTabsUIDelegateMock> open_tabs_ui_delegate_;
  testing::NiceMock<GlobalIdMapperMock> global_id_mapper_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;

  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;

  syncer::TestSyncService* sync_service_;
  sync_sessions::SyncedSession sync_session_;
  std::vector<const sync_sessions::SyncedSession*> sessions_;

  sessions::SessionTab open_tab_;
  std::vector<const sessions::SessionTab*> open_tabs_;

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

// Makes sure that the app don't crash when checking cells after -stop is
// called. This is done to prevent crbug.com/1469608 to regress.
TEST_F(RecentTabsTableCoordinatorTest, TestLoadFaviconAfterDisconnect) {
  SetupSyncState(YES, YES, YES, YES);
  CreateController();

  UINavigationController* navigation_controller =
      base::apple::ObjCCastStrict<UINavigationController>(
          base_view_controller_.presentedViewController);

  RecentTabsTableViewController* view_controller =
      base::apple::ObjCCastStrict<RecentTabsTableViewController>(
          navigation_controller.topViewController);

  [coordinator_ stop];

  for (NSInteger section_index = 0;
       section_index <
       [view_controller numberOfSectionsInTableView:view_controller.tableView];
       section_index++) {
    for (NSInteger row_index = 0;
         row_index < [view_controller tableView:view_controller.tableView
                          numberOfRowsInSection:section_index];
         row_index++) {
      [view_controller tableView:view_controller.tableView
           cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row_index
                                                    inSection:section_index]];
    }
  }
}

// It's possible to tap on the history sync promo action button to show a new
// history sync screen, while the dismiss animation is being played for the
// previous History Sync screen.
// This test verifies that there's no crash in this case.
// See https://crbug.com/1470860.
TEST_F(RecentTabsTableCoordinatorTest, TestReopenHistorySyncWhenPreviousShown) {
  SetupSyncState(YES, NO, NO, NO);
  // Disable history and tabs settings to ensure that the History Sync
  // coordinator.
  sync_service_->GetUserSettings()->SetSelectedTypes(
      /* sync_everything */ false, {});
  CreateController();

  id<RecentTabsPresentationDelegate> delegate =
      static_cast<id<RecentTabsPresentationDelegate>>(coordinator_);
  [delegate showHistorySyncOptInAfterDedicatedSignIn:NO];
  [delegate showHistorySyncOptInAfterDedicatedSignIn:NO];
}

}  // namespace
