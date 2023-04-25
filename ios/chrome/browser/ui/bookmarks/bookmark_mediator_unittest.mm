// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
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
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

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

NSString* const kFolderName = @"folder name";
NSString* const kDefaultFolderName = @"default folder";
NSString* const kEmail = @"foo1@gmail.com";

class BookmarkMediatorUnitTest : public BookmarkIOSUnitTestSupport {
  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();
    authentication_service_ = AuthenticationServiceFactory::GetForBrowserState(
        chrome_browser_state_.get());
    sync_service_ =
        SyncServiceFactory::GetForBrowserState(chrome_browser_state_.get());
    sync_setup_service_ = std::make_unique<FakeSyncSetupService>(sync_service_);

    mediator_ = [[BookmarkMediator alloc]
        initWithWithProfileBookmarkModel:profile_bookmark_model_
                    accountBookmarkModel:nullptr
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

  NSString* GetSavedToDeviceText(int count) {
    std::u16string pattern =
        l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count);
    return base::SysUTF16ToNSString(message);
  }

  NSString* GetSavedToFolderText(int count, NSString* folder_name) {
    std::u16string pattern =
        l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "title",
        base::SysNSStringToUTF16(folder_name));
    return base::SysUTF16ToNSString(message);
  }

  NSString* GetSavedToAccountText(int count, NSString* email) {
    std::u16string pattern =
        l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "email", base::SysNSStringToUTF16(email));
    return base::SysUTF16ToNSString(message);
  }

  NSString* GetSavedToFolderToAccountText(int count,
                                          NSString* folder_name,
                                          NSString* email) {
    std::u16string pattern = l10n_util::GetStringUTF16(
        IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT_FOLDER);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "title", base::SysNSStringToUTF16(folder_name),
        "email", base::SysNSStringToUTF16(email));
    return base::SysUTF16ToNSString(message);
  }

  BookmarkMediator* mediator_;
  ChromeAccountManagerService* account_manager_service_;
  AuthenticationService* authentication_service_;
  std::unique_ptr<FakeSyncSetupService> sync_setup_service_;
  syncer::SyncService* sync_service_;
  base::test::ScopedFeatureList scope_;
};

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedOutNoFolder) {
  constexpr int bookmark_count = 1;
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:nil
                                                     count:bookmark_count],
              GetSavedToDeviceText(bookmark_count));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedOutNoFolder) {
  constexpr int bookmark_count = 1;
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:nil
                                                     count:bookmark_count],
              GetSavedToDeviceText(bookmark_count));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedOutDefaultFolder) {
  constexpr int bookmark_count = 1;
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:kDefaultFolderName
                                                     count:bookmark_count],
              GetSavedToDeviceText(bookmark_count));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedOutDefaultFolder) {
  constexpr int bookmark_count = 1;
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:kDefaultFolderName
                                                     count:bookmark_count],
              GetSavedToDeviceText(bookmark_count));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedOutInFolder) {
  constexpr int bookmark_count = 1;
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:YES
                                                     title:kFolderName
                                                     count:bookmark_count],
              GetSavedToFolderText(bookmark_count, kFolderName));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedOutInFolder) {
  constexpr int bookmark_count = 1;
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:YES
                                                     title:kFolderName
                                                     count:bookmark_count],
              GetSavedToFolderText(bookmark_count, kFolderName));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedInNoFolder) {
  constexpr int bookmark_count = 1;
  SignInAndSync();
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:nil
                                                     count:bookmark_count],
              GetSavedToDeviceText(bookmark_count));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInNoFolder) {
  constexpr int bookmark_count = 1;
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:nil
                                                     count:bookmark_count],
              GetSavedToAccountText(bookmark_count, kEmail));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInNoFolderPlural) {
  constexpr int bookmark_count = 2;
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:nil
                                                     count:bookmark_count],
              GetSavedToAccountText(bookmark_count, kEmail));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedInDefaultFolder) {
  constexpr int bookmark_count = 1;
  SignInAndSync();
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:kDefaultFolderName
                                                     count:bookmark_count],
              GetSavedToDeviceText(bookmark_count));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInDefaultFolder) {
  constexpr int bookmark_count = 1;
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:kDefaultFolderName
                                                     count:bookmark_count],
              GetSavedToAccountText(bookmark_count, kEmail));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInDefaultFolderPlural) {
  constexpr int bookmark_count = 2;
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:NO
                                                     title:kDefaultFolderName
                                                     count:bookmark_count],
              GetSavedToAccountText(bookmark_count, kEmail));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagDisabledSignedInInFolder) {
  constexpr int bookmark_count = 1;
  SignInAndSync();
  setEmailInSnackbarFlag(false);
  ASSERT_NSEQ([mediator_ messageForAddingBookmarksInFolder:YES
                                                     title:kFolderName
                                                     count:bookmark_count],
              GetSavedToFolderText(bookmark_count, kFolderName));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInInFolder) {
  constexpr int bookmark_count = 1;
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ(
      [mediator_ messageForAddingBookmarksInFolder:YES
                                             title:kFolderName
                                             count:bookmark_count],
      GetSavedToFolderToAccountText(bookmark_count, kFolderName, kEmail));
}

TEST_F(BookmarkMediatorUnitTest, TestFlagEnabledSignedInInFolderPlural) {
  constexpr int bookmark_count = 2;
  SignInAndSync();
  setEmailInSnackbarFlag(true);
  ASSERT_NSEQ(
      [mediator_ messageForAddingBookmarksInFolder:YES
                                             title:kFolderName
                                             count:bookmark_count],
      GetSavedToFolderToAccountText(bookmark_count, kFolderName, kEmail));
}

}  // namespace
