// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "components/bookmarks/browser/bookmark_model.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"

namespace {

class BookmarksFolderChooserMediatorUnitTest
    : public BookmarkIOSUnitTestSupport {
 public:
  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();

    bookmark_model_->AddFolder(/*parent=*/bookmark_model_->mobile_node(),
                               /*index=*/0, u"Local folder");
    bookmark_model_->AddFolder(
        /*parent=*/bookmark_model_->account_mobile_node(),
        /*index=*/0, u"Account folder");

    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    system_identity_manager->AddIdentity(fake_identity);
    authentication_service_->SignIn(
        fake_identity,
        signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER);

    sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

    mediator_ = [[BookmarksFolderChooserMediator alloc]
        initWithBookmarkModel:bookmark_model_
                  editedNodes:{}
        authenticationService:authentication_service_
                  syncService:&sync_service_];
  }

  void TearDown() override { [mediator_ disconnect]; }

 protected:
  BookmarksFolderChooserMediator* mediator_;
  raw_ptr<AuthenticationService> authentication_service_;
  syncer::TestSyncService sync_service_;
};

TEST_F(BookmarksFolderChooserMediatorUnitTest,
       ShouldDisplayCloudIconForLocalOrSyncableBookmarks) {
  EXPECT_TRUE([mediator_ shouldDisplayCloudIconForLocalOrSyncableBookmarks]);

  // Mimic signout.
  bookmark_model_->RemoveAccountPermanentFolders();
  sync_service_.SetSignedOut();

  EXPECT_FALSE([mediator_ shouldDisplayCloudIconForLocalOrSyncableBookmarks]);
}

TEST_F(BookmarksFolderChooserMediatorUnitTest, ShouldShowAccountBookmarks) {
  ASSERT_NE(nullptr, bookmark_model_->account_mobile_node());
  EXPECT_TRUE([mediator_ shouldShowAccountBookmarks]);
  bookmark_model_->RemoveAccountPermanentFolders();
  ASSERT_EQ(nullptr, bookmark_model_->account_mobile_node());
  EXPECT_FALSE([mediator_ shouldShowAccountBookmarks]);
}

}  // namespace
