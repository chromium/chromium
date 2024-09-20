// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/i18n/message_formatter.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/url_and_title.h"
#import "components/signin/public/base/consent_level.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_client_impl.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

NSString* const kFolderName = @"folder name";
NSString* const kEmail = @"foo1@gmail.com";

// List of cases to tests.
enum class SignInStatus {
  // The user is signed out.
  kSignOut,
  // The user is signed in and using the local or syncable storage.
  kSignedInOnlyWithLocalOrSyncableStorage,
  // The user is signed in, not syncing bookmark.
  kSignedInNoBookmarkSyncing,
  // The user is signed in and using the account storage.
  kSignedInOnlyWithAccountStorage,
  // The user is signed in and syncing.
  KSignedInAndSync
};

class BookmarkMediatorUnitTest
    : public BookmarkIOSUnitTestSupport,
      public testing::WithParamInterface<
          std::tuple<int, bool, SignInStatus, bool>> {
 public:
  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();
    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_.get());

    mediator_ =
        [[BookmarkMediator alloc] initWithBookmarkModel:bookmark_model_.get()
                                                  prefs:profile_->GetPrefs()
                                  authenticationService:authentication_service_
                                            syncService:&sync_service_];
  }

  // Number of bookmark saved.
  int GetBookmarkCountParam() { return std::get<0>(GetParam()); }
  // Whether the bookmarks are saved in the default folder or not.
  bool GetFolderWasSelectedByUserParam() { return std::get<1>(GetParam()); }
  SignInStatus GetSignInStatusParam() { return std::get<2>(GetParam()); }
  // Whether to display the number of bookmarks when it’s not 1.
  bool GetDisplayTheNumberOfBookmarksParam() { return std::get<3>(GetParam()); }

 protected:
  // Signs in using `fakeIdentity1`.
  FakeSystemIdentity* SignInOnly() {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    system_identity_manager->AddIdentity(fake_identity);
    authentication_service_->SignIn(
        fake_identity,
        signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER);
    sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
    return fake_identity;
  }

  // Signs in using `fakeIdentity1`. Disables synchronization of bookmark.
  FakeSystemIdentity* SignInOnlyAndDisableBookmark() {
    FakeSystemIdentity* fake_identity = SignInOnly();
    sync_service_.GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kBookmarks, NO);
    return fake_identity;
  }

  // Signs in and enable sync, using the same identity than `SignInOnly()`.
  void SignInAndSync() {
    FakeSystemIdentity* fake_identity = SignInOnly();
    authentication_service_->GrantSyncConsent(
        fake_identity,
        signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER);
    sync_service_.SetSignedIn(signin::ConsentLevel::kSync);
  }

  // Returns `IDS_IOS_BOOKMARKS_BULK_SAVED` string with `count` value.
  NSString* GetSavedToDeviceText(int count, bool show_count) {
    return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
        (show_count) ? IDS_IOS_BOOKMARKS_BULK_SAVED : IDS_IOS_BOOKMARKS_SAVED,
        count));
  }

  // Returns `IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER` string with `count` and
  // `folder_name` value.
  NSString* GetSavedToFolderText(int count,
                                 NSString* folder_name,
                                 bool show_count) {
    std::u16string pattern = l10n_util::GetStringUTF16(
        (show_count) ? IDS_IOS_BOOKMARK_PAGE_BULK_SAVED_FOLDER
                     : IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "title",
        base::SysNSStringToUTF16(folder_name));
    return base::SysUTF16ToNSString(message);
  }

  // Returns `IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT` string with `count` and
  // `email` value.
  NSString* GetSavedToAccountText(int count, NSString* email, bool show_count) {
    std::u16string pattern = l10n_util::GetStringUTF16(
        (show_count) ? IDS_IOS_BOOKMARK_PAGE_BULK_SAVED_INTO_ACCOUNT
                     : IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "email", base::SysNSStringToUTF16(email));
    return base::SysUTF16ToNSString(message);
  }

  // Returns `IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT` string with `count`,
  // `folder_name` and `email` value.
  NSString* GetSavedToFolderToAccountText(int count,
                                          NSString* folder_name,
                                          NSString* email,
                                          bool show_count) {
    std::u16string pattern = l10n_util::GetStringUTF16(
        (show_count) ? IDS_IOS_BOOKMARK_PAGE_BULK_SAVED_INTO_ACCOUNT_FOLDER
                     : IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT_FOLDER);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "title", base::SysNSStringToUTF16(folder_name),
        "email", base::SysNSStringToUTF16(email));
    return base::SysUTF16ToNSString(message);
  }

  // Returns `IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER_TO_DEVICE` string with `count`,
  // `folder_name` and `email` value.
  NSString* GetSavedLocallyOnlyText(int count,
                                    NSString* folder_name,
                                    bool show_count) {
    std::u16string pattern = l10n_util::GetStringUTF16(
        (show_count) ? IDS_IOS_BOOKMARK_PAGE_BULK_SAVED_FOLDER_TO_DEVICE
                     : IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER_TO_DEVICE);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "title",
        base::SysNSStringToUTF16(folder_name));
    return base::SysUTF16ToNSString(message);
  }

  BookmarkMediator* mediator_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  raw_ptr<AuthenticationService> authentication_service_;
  syncer::TestSyncService sync_service_;
  base::test::ScopedFeatureList scope_;
  base::HistogramTester histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BookmarkMediatorUnitTest,
    testing::Combine(
        // Number of bookmarked saved.
        testing::Values(1, 2),
        // Bookmark saved in the default folder or not.
        testing::Bool(),
        // Sign-in status.
        testing::Values(SignInStatus::kSignOut,
                        SignInStatus::kSignedInOnlyWithLocalOrSyncableStorage,
                        SignInStatus::kSignedInNoBookmarkSyncing,
                        SignInStatus::kSignedInOnlyWithAccountStorage,
                        SignInStatus::KSignedInAndSync),
        // Whether or not the count should be displayed.
        testing::Bool()));

// Tests the snackbar message with all the different combinaisons with:
// * One or two saved bookmarks
// * Using the default folder or not
// * Being signed-out/signed in/signed in with account storage/signed in + sync.
TEST_P(BookmarkMediatorUnitTest, TestSnackBarMessage) {
  const int bookmark_count = GetBookmarkCountParam();
  const SignInStatus signed_in_status = GetSignInStatusParam();
  const bool folder_was_selected_by_user = GetFolderWasSelectedByUserParam();
  NSString* expected_snackbar_message = nil;
  const bookmarks::BookmarkNode* ancestor_permanent_folder =
      bookmark_model_->mobile_node();
  bool show_count = GetDisplayTheNumberOfBookmarksParam();
  switch (signed_in_status) {
    case SignInStatus::kSignedInNoBookmarkSyncing:
      SignInOnlyAndDisableBookmark();
      [[fallthrough]];
    case SignInStatus::kSignOut:
      expected_snackbar_message =
          (folder_was_selected_by_user)
              ? GetSavedToFolderText(bookmark_count, kFolderName, show_count)
              : GetSavedToDeviceText(bookmark_count, show_count);
      break;
    case SignInStatus::kSignedInOnlyWithLocalOrSyncableStorage:
      if (!folder_was_selected_by_user) {
        // If the user is signed-in, syncing bookmarks, the default folder is
        // the account folder. This case can’t occur, so there is nothing to
        // test.
        return;
      }
      expected_snackbar_message =
          GetSavedLocallyOnlyText(bookmark_count, kFolderName, show_count);
      SignInOnly();
      break;
    case SignInStatus::kSignedInOnlyWithAccountStorage:
      expected_snackbar_message =
          (folder_was_selected_by_user)
              ? GetSavedToFolderToAccountText(bookmark_count, kFolderName,
                                              kEmail, show_count)
              : GetSavedToAccountText(bookmark_count, kEmail, show_count);
      SignInOnly();
      ancestor_permanent_folder = bookmark_model_->account_mobile_node();
      break;
    case SignInStatus::KSignedInAndSync:
      expected_snackbar_message =
          (folder_was_selected_by_user)
              ? GetSavedToFolderToAccountText(bookmark_count, kFolderName,
                                              kEmail, show_count)
              : GetSavedToAccountText(bookmark_count, kEmail, show_count);
      SignInAndSync();
      // This test requires that the initial download of bookmarks completed.
      static_cast<BookmarkClientImpl*>(bookmark_model_->client())
          ->SetIsSyncFeatureEnabledIncludingBookmarksForTest();
      break;
  }

  const bookmarks::BookmarkNode* parent_folder = AddFolder(
      ancestor_permanent_folder, base::SysNSStringToUTF16(kFolderName));

  NSString* const snackbar_message =
      bookmark_utils_ios::messageForAddingBookmarksInFolder(
          parent_folder, bookmark_model_.get(), folder_was_selected_by_user,
          show_count, bookmark_count,
          AuthenticationServiceFactory::GetForProfile(profile_.get())
              ->GetWeakPtr(),
          &sync_service_);
  ASSERT_NSEQ(snackbar_message, expected_snackbar_message);
}

// Tests bulkAddBookmarksWithURLs with no valid URL passed.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageNoValidURLs) {
  NSArray* URLs = @[ [[NSURL alloc] initWithString:@""] ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(0U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text, @"0 bookmarks saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 0, 1);
}

// Tests bulkAddBookmarksWithURLs with one valid URL passed.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageOneValidURL) {
  NSArray* URLs = @[ [[NSURL alloc] initWithString:@"https://google.ca"] ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(1U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text, @"Bookmark saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 1, 1);
}

// Tests bulkAddBookmarksWithURLs with two valid URLs passed.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageTwoValidURLs) {
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"https://google.fr"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(2U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text, @"2 bookmarks saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 2, 1);
}

// Tests bulkAddBookmarksWithURLs with a set of mixed valid and invalid URLs.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageValidAndInvalidURLs) {
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"::invalid::"],
    [[NSURL alloc] initWithString:@"https://google.fr"],
    [[NSURL alloc] initWithString:@"https://google.co.jp"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text, @"3 bookmarks saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);
}

// Tests bulkAddBookmarksWithURLs with duplicate bookmarks.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageDuplicateBookmarks) {
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"::invalid::"],
    [[NSURL alloc] initWithString:@"https://google.fr"],
    [[NSURL alloc] initWithString:@"https://google.co.jp"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text, @"3 bookmarks saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);

  // Try bulk adding the same URLs again, none should be added.
  MDCSnackbarMessage* const snackbarMessageDuplicates =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks_dupes =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks_dupes.size());
  ASSERT_NSEQ(snackbarMessageDuplicates.text, @"0 bookmarks saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 0, 1);
}

// Tests bulkAddBookmarksWithURLs with no valid URL passed while signed in and
// syncing.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageNoValidURLsSyncing) {
  SignInAndSync();
  NSArray* URLs = @[ [[NSURL alloc] initWithString:@""] ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(0U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text,
              @"0 bookmarks saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 0, 1);
}

// Tests bulkAddBookmarksWithURLs with one valid URL passed while signed in and
// syncing.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageOneValidURLSyncing) {
  SignInAndSync();
  NSArray* URLs = @[ [[NSURL alloc] initWithString:@"https://google.ca"] ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(1U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text,
              @"Bookmark saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 1, 1);
}

// Tests bulkAddBookmarksWithURLs with two valid URLs passed while signed in and
// syncing.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageTwoValidURLsSyncing) {
  SignInAndSync();
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"https://google.fr"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(2U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text,
              @"2 bookmarks saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 2, 1);
}

// Tests bulkAddBookmarksWithURLs with a set of mixed valid and invalid URLs
// while signed in and syncing.
TEST_F(BookmarkMediatorUnitTest,
       TestBulkSnackbarMessageValidAndInvalidURLsSyncing) {
  SignInAndSync();
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"::invalid::"],
    [[NSURL alloc] initWithString:@"https://google.fr"],
    [[NSURL alloc] initWithString:@"https://google.co.jp"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text,
              @"3 bookmarks saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);
}

// Tests bulkAddBookmarksWithURLs with duplicate bookmarks while signed in and
// syncing.
TEST_F(BookmarkMediatorUnitTest,
       TestBulkSnackbarMessageDuplicateBookmarksSyncing) {
  SignInAndSync();
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"::invalid::"],
    [[NSURL alloc] initWithString:@"https://google.fr"],
    [[NSURL alloc] initWithString:@"https://google.co.jp"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text,
              @"3 bookmarks saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);

  // Try bulk adding the same URLs again, none should be added.
  MDCSnackbarMessage* const snackbarMessageDuplicates =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks_dupes =
      bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks_dupes.size());
  ASSERT_NSEQ(snackbarMessageDuplicates.text,
              @"0 bookmarks saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 0, 1);
}

}  // namespace
