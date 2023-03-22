// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/bookmarks/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarkMediator ()
- (NSString*)messageForAddingBookmarksInFolder:(BOOL)addFolder
                                         title:(NSString*)folderTitle
                                         count:(int)count;
@end

class FakeSyncSetupService : public SyncSetupService {
 public:
  FakeSyncSetupService(syncer::SyncService* sync_service)
      : SyncSetupService(sync_service) {}

  bool IsDataTypePreferred(syncer::ModelType datatype) const override {
    return true;
  }
};

namespace {

class BookmarkMediatorUnitTest : public BookmarkIOSUnitTestSupport {
  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();
    authentication_service_ = AuthenticationServiceFactory::GetForBrowserState(
        chrome_browser_state_.get());
    sync_service_ =
        SyncServiceFactory::GetForBrowserState(chrome_browser_state_.get());
    sync_setup_service_ = std::make_unique<FakeSyncSetupService>(sync_service_);

    mediator_ = [[BookmarkMediator alloc]
        initWithWithBookmarkModel:bookmark_model_
                            prefs:chrome_browser_state_->GetPrefs()
            authenticationService:authentication_service_
                 syncSetupService:sync_setup_service_.get()];
  }

 protected:
  void SignInAndSync() {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    system_identity_manager->AddIdentity(fake_identity);
    authentication_service_->SignIn(fake_identity);
    authentication_service_->GrantSyncConsent(fake_identity);
  }

  void setEmailInSnackbarFlag(bool enabled) {
    if (enabled) {
      scope_.InitWithFeatures({kEnableEmailInBookmarksReadingListSnackbar}, {});
    } else {
      scope_.InitWithFeatures({}, {kEnableEmailInBookmarksReadingListSnackbar});
    }
  }

  BookmarkMediator* mediator_;
  ChromeAccountManagerService* account_manager_service_;
  AuthenticationService* authentication_service_;
  std::unique_ptr<FakeSyncSetupService> sync_setup_service_;
  syncer::SyncService* sync_service_;
  base::test::ScopedFeatureList scope_;
};

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedOutNoFolder) {
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:nil
                                                     count:1],
              @"Bookmarked");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedOutNoFolder) {
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:nil
                                                     count:1],
              @"Bookmarked");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedOutDefaultFolder) {
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:@"default folder"
                                                     count:1],
              @"Bookmarked");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedOutDefaultFolder) {
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:@"default folder"
                                                     count:1],
              @"Bookmarked");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedOutInFolder) {
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:YES
                                                     title:@"folder name"
                                                     count:1],
              @"Bookmarked to folder name");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedOutInFolder) {
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:YES
                                                     title:@"folder name"
                                                     count:1],
              @"Bookmarked to folder name");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedInNoFolder) {
  SignInAndSync();
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:nil
                                                     count:1],
              @"Bookmarked");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInNoFolder) {
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:nil
                                                     count:1],
              @"Bookmark saved in your account, foo1@gmail.com");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInNoFolderPlural) {
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:nil
                                                     count:2],
              @"Bookmarks saved in your account, foo1@gmail.com");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedInDefaultFolder) {
  SignInAndSync();
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:@"default folder"
                                                     count:1],
              @"Bookmarked");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInDefaultFolder) {
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:@"default folder"
                                                     count:1],
              @"Bookmark saved in your account, foo1@gmail.com");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInDefaultFolderPlural) {
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:@"default folder"
                                                     count:2],
              @"Bookmarks saved in your account, foo1@gmail.com");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedInInFolder) {
  SignInAndSync();
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:YES
                                                     title:@"folder name"
                                                     count:1],
              @"Bookmarked to folder name");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInInFolder) {
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:YES
                                                     title:@"folder name"
                                                     count:1],
              @"Bookmark saved to folder name in your account, foo1@gmail.com");
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInInFolderPlural) {
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ(
      [mediator_ messageForAddingBookmarksInFolder:YES
                                             title:@"folder name"
                                             count:2],
      @"Bookmarks saved to folder name in your account, foo1@gmail.com");
}

}  // namespace
