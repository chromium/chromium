// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "components/sessions/core/serialized_navigation_entry_test_helper.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/primary_account_mutator.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/fake_data_type_controller_delegate.h"
#import "components/sync/test/test_sync_service.h"
#import "components/sync/test/test_sync_user_settings.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_sessions/synced_session.h"
#import "components/sync_user_events/global_id_mapper.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/recent_tabs/sessions_sync_user_state.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
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
               base::WeakPtr<syncer::DataTypeControllerDelegate>());
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

  MOCK_METHOD1(GetAllForeignSessions,
               bool(std::vector<raw_ptr<const sync_sessions::SyncedSession,
                                        VectorExperimental>>* sessions));
  MOCK_METHOD3(GetForeignTab,
               bool(const std::string& tag,
                    const SessionID tab_id,
                    const sessions::SessionTab** tab));
  MOCK_METHOD1(DeleteForeignSession, void(const std::string& tag));
  MOCK_METHOD1(
      GetForeignSession,
      std::vector<const sessions::SessionWindow*>(const std::string& tag));
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

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::FaviconServiceFactory::GetInstance(),
                              ios::FaviconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());

    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeSyncServiceFactory));

    builder.AddTestingFactory(
        SessionSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            &BuildMockSessionSyncServiceForRecentTabsTableCoordinator));
    builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

    FakeStartupInformation* startup_information_ =
        [[FakeStartupInformation alloc] init];
    app_state_ =
        [[AppState alloc] initWithStartupInformation:startup_information_];
    scene_state_ = [[SceneState alloc] initWithAppState:app_state_];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
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
            SessionSyncServiceFactory::GetForProfile(profile_.get()));

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));

    if (!signed_in) {
      CHECK(!sync_enabled);
      CHECK(!sync_completed);
      CHECK(!has_foreign_sessions);
      sync_service_->SetSignedOut();
    } else if (!sync_enabled) {
      CHECK(!sync_completed);
      CHECK(!has_foreign_sessions);
      sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);
    } else {
      sync_service_->SetSignedIn(signin::ConsentLevel::kSync);
      if (!sync_completed) {
        sync_service_->GetUserSettings()
            ->ClearInitialSyncFeatureSetupComplete();
      }
    }

    // Needed by SyncService's initialization.
    ON_CALL(*session_sync_service, GetControllerDelegate())
        .WillByDefault(Return(fake_controller_delegate_.GetWeakPtr()));
    ON_CALL(*session_sync_service, GetGlobalIdMapper())
        .WillByDefault(Return(&global_id_mapper_));

    if (signed_in) {
      AuthenticationService* authentication_service =
          AuthenticationServiceFactory::GetForProfile(profile_.get());

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
    mock_settings_commands_handler_ =
        [OCMockObject mockForProtocol:@protocol(SettingsCommands)];

    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_commands_handler_
                     forProtocol:@protocol(SettingsCommands)];

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
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  syncer::FakeDataTypeControllerDelegate fake_controller_delegate_;
  testing::NiceMock<OpenTabsUIDelegateMock> open_tabs_ui_delegate_;
  testing::NiceMock<GlobalIdMapperMock> global_id_mapper_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  AppState* app_state_;
  SceneState* scene_state_;

  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;

  raw_ptr<syncer::TestSyncService> sync_service_;
  sync_sessions::SyncedSession sync_session_;
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions_;

  sessions::SessionTab open_tab_;
  std::vector<const sessions::SessionTab*> open_tabs_;

  // Must be declared *after* `profile_` so it can outlive it.
  RecentTabsCoordinator* coordinator_;
  id<ApplicationCommands> mock_application_commands_handler_;
  id<SettingsCommands> mock_settings_commands_handler_;
};

TEST_F(RecentTabsTableCoordinatorTest, TestConstructorDestructor) {
  SetupSyncState(NO, NO, NO, NO);
  CreateController();
  EXPECT_TRUE(coordinator_);
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedOut) {
  // TODO(crbug.com/40603410): Actual test expectations are missing below.
  SetupSyncState(NO, NO, NO, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncOff) {
  // TODO(crbug.com/40603410): Actual test expectations are missing below.
  SetupSyncState(YES, NO, NO, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncInProgress) {
  // TODO(crbug.com/40603410): Actual test expectations are missing below.
  SetupSyncState(YES, YES, NO, NO);
  CreateController();
}
TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncOnWithoutSessions) {
  // TODO(crbug.com/40603410): Actual test expectations are missing below.
  SetupSyncState(YES, YES, YES, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncOnWithSessions) {
  // TODO(crbug.com/40603410): Actual test expectations are missing below.
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
