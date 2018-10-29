// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "components/browser_sync/profile_sync_service.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/recent_tabs/sessions_sync_user_state.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using testing::AtLeast;
using testing::Return;

namespace {

std::unique_ptr<KeyedService> CreateSyncSetupService(
    web::BrowserState* context) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForBrowserState(chrome_browser_state);
  return std::make_unique<SyncSetupServiceMock>(
      sync_service, chrome_browser_state->GetPrefs());
}

class ProfileSyncServiceMockForRecentTabsTableCoordinator
    : public browser_sync::ProfileSyncServiceMock {
 public:
  explicit ProfileSyncServiceMockForRecentTabsTableCoordinator(
      InitParams init_params)
      : browser_sync::ProfileSyncServiceMock(std::move(init_params)) {}
  ~ProfileSyncServiceMockForRecentTabsTableCoordinator() override {}

  MOCK_METHOD0(GetOpenTabsUIDelegate, sync_sessions::OpenTabsUIDelegate*());
};

std::unique_ptr<KeyedService>
BuildMockProfileSyncServiceForRecentTabsTableCoordinator(
    web::BrowserState* context) {
  return std::make_unique<ProfileSyncServiceMockForRecentTabsTableCoordinator>(
      CreateProfileSyncServiceParamsForTest(
          nullptr, ios::ChromeBrowserState::FromBrowserState(context)));
}

class OpenTabsUIDelegateMock : public sync_sessions::OpenTabsUIDelegate {
 public:
  OpenTabsUIDelegateMock() {}
  ~OpenTabsUIDelegateMock() override {}

  MOCK_CONST_METHOD2(GetSyncedFaviconForPageURL,
                     bool(const std::string& pageurl,
                          scoped_refptr<base::RefCountedMemory>* favicon_png));
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

class RecentTabsTableCoordinatorTest : public BlockCleanupTest {
 public:
  RecentTabsTableCoordinatorTest() : no_error_(GoogleServiceAuthError::NONE) {}

 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&CreateSyncSetupService));
    test_cbs_builder.AddTestingFactory(
        ProfileSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            &BuildMockProfileSyncServiceForRecentTabsTableCoordinator));
    chrome_browser_state_ = test_cbs_builder.Build();

    ProfileSyncServiceMockForRecentTabsTableCoordinator* sync_service =
        static_cast<ProfileSyncServiceMockForRecentTabsTableCoordinator*>(
            ProfileSyncServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));
    EXPECT_CALL(*sync_service, GetAuthError())
        .WillRepeatedly(::testing::ReturnRef(no_error_));
    ON_CALL(*sync_service, GetRegisteredDataTypes())
        .WillByDefault(Return(syncer::ModelTypeSet()));
    sync_service->Initialize();
    EXPECT_CALL(*sync_service, GetTransportState())
        .WillRepeatedly(Return(syncer::SyncService::TransportState::ACTIVE));
    EXPECT_CALL(*sync_service, GetOpenTabsUIDelegate())
        .WillRepeatedly(Return(nullptr));
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;

    BlockCleanupTest::TearDown();
  }

  void SetupSyncState(BOOL signedIn,
                      BOOL syncEnabled,
                      BOOL hasForeignSessions) {
    SigninManager* siginManager = ios::SigninManagerFactory::GetForBrowserState(
        chrome_browser_state_.get());
    if (signedIn)
      siginManager->SetAuthenticatedAccountInfo("test", "test");
    else if (siginManager->IsAuthenticated())
      siginManager->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);

    SyncSetupServiceMock* syncSetupService = static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
    EXPECT_CALL(*syncSetupService, IsSyncEnabled())
        .WillRepeatedly(Return(syncEnabled));
    EXPECT_CALL(*syncSetupService, IsDataTypePreferred(syncer::PROXY_TABS))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*syncSetupService, GetSyncServiceState())
        .WillRepeatedly(Return(SyncSetupService::kNoSyncServiceError));

    if (syncEnabled) {
      ProfileSyncServiceMockForRecentTabsTableCoordinator* sync_service =
          static_cast<ProfileSyncServiceMockForRecentTabsTableCoordinator*>(
              ProfileSyncServiceFactory::GetForBrowserState(
                  chrome_browser_state_.get()));
      open_tabs_ui_delegate_.reset(new OpenTabsUIDelegateMock());
      EXPECT_CALL(*sync_service, GetOpenTabsUIDelegate())
          .WillRepeatedly(Return(open_tabs_ui_delegate_.get()));
      EXPECT_CALL(*open_tabs_ui_delegate_, GetAllForeignSessions(_))
          .WillRepeatedly(Return(hasForeignSessions));
    }
  }

  void CreateController() {
    // Sets up the test expectations for the Sync Service Observer Bridge.
    // RecentTabsTableCoordinator must be added as an observer of
    // ProfileSyncService changes and removed when it is destroyed.
    browser_sync::ProfileSyncServiceMock* sync_service =
        static_cast<browser_sync::ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));
    EXPECT_CALL(*sync_service, AddObserver(_)).Times(AtLeast(1));
    EXPECT_CALL(*sync_service, RemoveObserver(_)).Times(AtLeast(1));
    coordinator_ = [[RecentTabsCoordinator alloc]
        initWithBaseViewController:nil
                      browserState:chrome_browser_state_.get()];
    [coordinator_ start];
  }

 protected:
  web::TestWebThreadBundle thread_bundle_;
  GoogleServiceAuthError no_error_;
  IOSChromeScopedTestingLocalState local_state_;

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<OpenTabsUIDelegateMock> open_tabs_ui_delegate_;

  // Must be declared *after* |chrome_browser_state_| so it can outlive it.
  RecentTabsCoordinator* coordinator_;
};

TEST_F(RecentTabsTableCoordinatorTest, TestConstructorDestructor) {
  CreateController();
  EXPECT_TRUE(coordinator_);
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedOut) {
  SetupSyncState(NO, NO, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncOff) {
  SetupSyncState(YES, NO, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncInProgress) {
  SetupSyncState(YES, YES, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncOnWithSessions) {
  SetupSyncState(YES, YES, YES);
  CreateController();
}

}  // namespace
