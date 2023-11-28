// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/i18n/message_formatter.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/common/storage_type.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Expects a batch upload dialog item on the current screen with message
// formatted for `count` local bookmarks and `email` user email.
void ExpectBatchUploadSection(int count, NSString* email) {
  // Verify that the batch upload section is visible.
  NSString* text = nil;
  NSString* detailText = base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_SECTION_DESCRIPTION),
          "count", count, "email", base::SysNSStringToUTF16(email)));
  // Build label for a TableViewImageItem.
  NSString* label = [NSString stringWithFormat:@"%@, %@", text, detailText];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kBookmarksHomeBatchUploadRecommendationItemIdentifier),
                     grey_accessibilityLabel(label), grey_sufficientlyVisible(),
                     nil)] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kBookmarksHomeBatchUploadButtonIdentifier),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Verifies that the batch upload section is not visible.
void ExpectNoBatchUploadDialog() {
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kBookmarksHomeBatchUploadRecommendationItemIdentifier),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(kBookmarksHomeBatchUploadButtonIdentifier),
              grey_sufficientlyVisible(), nil)] assertWithMatcher:grey_nil()];
}

// Verifies that the batch upload alert (action sheet) is visible, with a
// message formatted for `count` number of local bookmarks.
void ExpectBatchUploadAlert(int count) {
  NSString* alertTitle = l10n_util::GetPluralNSStringF(
      IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_ALERT_TITLE, count);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(alertTitle),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::AlertAction(l10n_util::GetNSString(
                         IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_ALERT_BUTTON)),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  // No checks for the "cancel" button since the cancel button is not shown on
  // iPads.
}

// Verifies that no batch upload alert is displayed.
void ExpectNoBatchUploadAlert() {
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::AlertAction(l10n_util::GetNSString(
                         IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_ALERT_BUTTON)),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Waits for snackbar item to show up after pressing save on the batch upload
// alert.
void ExpectBatchUploadConfirmationSnackbar(int count, NSString* email) {
  NSString* text = base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_SNACKBAR_MESSAGE),
          "count", count, "email", base::SysNSStringToUTF16(email)));
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       text)];
}

}  // namespace

@interface BookmarksBatchUploadTestCase : WebHttpServerChromeTestCase
@end

@implementation BookmarksBatchUploadTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(syncer::kEnableBookmarksAccountStorage);
  config.features_enabled.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
  return config;
}

- (void)setUp {
  [super setUp];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey clearBookmarks];
}

- (void)tearDown {
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
  [super tearDown];
}

@end

@interface BookmarksBatchUploadDisabledTestCase : BookmarksBatchUploadTestCase
@end

@implementation BookmarksBatchUploadDisabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_disabled.push_back(kEnableBatchUploadFromBookmarksManager);
  return config;
}

#pragma mark - BookmarksBatchUploadDisabledTestCase Tests

// Tests that the batch upload dialog is not shown when the feature is disabled.
- (void)testNoBatchUploadDialogIfFeatureDisabled {
  // Add one local bookmark.
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example1"
                       URL:@"https://www.example1.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Adds and signs in with `fakeIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [BookmarkEarlGreyUI openBookmarks];

  // Verify that the batch upload section is not visible.
  ExpectNoBatchUploadDialog();
}

@end

@interface BookmarksBatchUploadEnabledTestCase : BookmarksBatchUploadTestCase
@end

@implementation BookmarksBatchUploadEnabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kEnableBatchUploadFromBookmarksManager);
  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
}

- (void)tearDown {
  [super tearDown];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
}

#pragma mark - BookmarksBatchUploadEnabledTestCase Tests

// Tests that no batch upload dialog is shown if the user is not signed-in.
- (void)testNoBatchUploadDialogIfNotSignedIn {
  // Add one local bookmark.
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example1"
                       URL:@"https://www.example1.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  [BookmarkEarlGreyUI openBookmarks];

  // Verify that the batch upload section is not visible.
  ExpectNoBatchUploadDialog();

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:0
             forBucket:YES
          forHistogram:
              @"IOS.Bookmarks.BulkSaveBookmarksInAccountViewRecreated"],
      @"Invalid metric count.");
}

// Tests that no batch upload dialog is shown if there are no local bookmarks.
- (void)testNoBatchUploadDialogIfNoLocalBookmarks {
  // TODO(crbug.com/1451511): Add ASSERT for no local bookmarks.

  // Adds and signs in with `fakeIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [BookmarkEarlGreyUI openBookmarks];

  // Verify that the batch upload section is not visible.
  ExpectNoBatchUploadDialog();

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:0
             forBucket:YES
          forHistogram:
              @"IOS.Bookmarks.BulkSaveBookmarksInAccountViewRecreated"],
      @"Invalid metric count.");
}

// Tests that the batch upload dialog is shown and has the correct string for a
// single local bookmark.
- (void)testBatchUploadDialogTestIfSingleLocalBookmark {
  // Add one local bookmark.
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example1"
                       URL:@"https://www.example1.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Adds and signs in with `fakeIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [BookmarkEarlGreyUI openBookmarks];

  // Verify that the batch upload section is visible.
  ExpectBatchUploadSection(1, fakeIdentity.userEmail);

  // Verify that the metric count is non-zero.
  GREYAssertNotNil(
      [MetricsAppInterface
           expectCount:0
             forBucket:YES
          forHistogram:
              @"IOS.Bookmarks.BulkSaveBookmarksInAccountViewRecreated"],
      @"Invalid metric count.");
}

// Tests that the batch upload dialog is shown and has the correct string for
// multiple local bookmarks.
- (void)testBatchUploadDialogTextIfMultipleLocalBookmarks {
  // Add two local bookmarks.
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example1"
                       URL:@"https://www.example1.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example2"
                       URL:@"https://www.example2.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Adds and signs in with `fakeIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [BookmarkEarlGreyUI openBookmarks];

  // Verify that the batch upload section is visible.
  ExpectBatchUploadSection(2, fakeIdentity.userEmail);

  // Verify that the metric count is non-zero.
  GREYAssertNotNil(
      [MetricsAppInterface
           expectCount:0
             forBucket:YES
          forHistogram:
              @"IOS.Bookmarks.BulkSaveBookmarksInAccountViewRecreated"],
      @"Invalid metric count.");
}

// Tests that the batch upload dialog is removed if local bookmarks are removed
// behind the screen.
- (void)testBatchUploadDialogRemovedIfLocalBookmarkIsRemoved {
  // Add one local bookmark.
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example1"
                       URL:@"https://www.example1.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Adds and signs in with `fakeIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [BookmarkEarlGreyUI openBookmarks];

  // Verify that the batch upload section is visible.
  ExpectBatchUploadSection(1, fakeIdentity.userEmail);

  // Remove local bookmark directly in the model. This kinda portrays removing a
  // bookmark behind the screen (for eg. from another tab).
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"example1"
                    inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the batch upload dialog is removed.
  ExpectNoBatchUploadDialog();
}

// Tests that the batch upload dialog is updated if a local bookmark is removed
// on another screen.
- (void)testBatchUploadDialogUpdateIfLocalBookmarkIsRemoved {
  // Add two local bookmarks.
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example1"
                       URL:@"https://www.example1.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example2"
                       URL:@"https://www.example2.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Adds and signs in with `fakeIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [BookmarkEarlGreyUI openBookmarks];

  // Verify that the batch upload section is visible.
  ExpectBatchUploadSection(2, fakeIdentity.userEmail);

  // Remove local bookmark directly in the model. This kinda portrays removing a
  // bookmark on another screen (for eg. from another tab).
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"example1"
                    inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the batch upload dialog is updated.
  ExpectBatchUploadSection(1, fakeIdentity.userEmail);
}

// Tests that an action sheet is shown with the correct text upon clicking the
// batch upload button in bookmarks home.
- (void)testBatchUploadAlert {
  // Add one local bookmark.
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example1"
                       URL:@"https://www.example1.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Adds and signs in with `fakeIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [BookmarkEarlGreyUI openBookmarks];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeBatchUploadButtonIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that alert is visible.
  ExpectBatchUploadAlert(1);

  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@"IOS.Bookmarks.BulkSaveBookmarksInAccountCount"],
      @"Invalid metric count.");
}

// Tests that dismissing the action sheet (upon clicking the batch upload button
// in bookmarks home) does not alter the bookmarks home UI.
- (void)testBatchUploadAlertDismiss {
  // Add one local bookmark.
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example1"
                       URL:@"https://www.example1.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Adds and signs in with `fakeIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [BookmarkEarlGreyUI openBookmarks];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeBatchUploadButtonIdentifier)]
      performAction:grey_tap()];

  // Verify that alert is visible.
  ExpectBatchUploadAlert(1);

  // Tap anywhere outside the alert to dismiss it.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarksHomeNavigationBarDoneButtonIdentifier)]
      performAction:grey_tap()];

  // Verify that the alert is no longer visible.
  ExpectNoBatchUploadAlert();

  // Verify that the batch upload dialog is still visible.
  ExpectBatchUploadSection(1, fakeIdentity.userEmail);

  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@"IOS.Bookmarks.BulkSaveBookmarksInAccountCount"],
      @"Invalid metric count.");
}

// Tests that upon clicking on the "save" button on the batch upload alert, the
// alert is dismissed and the batch upload dialog has been removed.
- (void)testBatchUploadAlertConfirm {
  // Add one local bookmark.
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example1"
                       URL:@"https://www.example1.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Adds and signs in with `fakeIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [BookmarkEarlGreyUI openBookmarks];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeBatchUploadButtonIdentifier)]
      performAction:grey_tap()];

  // Verify that alert is visible.
  ExpectBatchUploadAlert(1);

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertAction(l10n_util::GetNSString(
                     IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_ALERT_BUTTON))]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that a snackbar is shown upon upload.
  ExpectBatchUploadConfirmationSnackbar(1, fakeIdentity.userEmail);

  // Verify that the alert is no longer visible.
  ExpectNoBatchUploadAlert();

  // Verify that the batch upload dialog is no longer visible.
  ExpectNoBatchUploadDialog();

  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:1
              forHistogram:@"IOS.Bookmarks.BulkSaveBookmarksInAccountCount"],
      @"Invalid metric count.");

  // TODO(crbug.com/1451511): Verify that the bookmarks have been moved to the
  // account model and the local bookmark model is empty.
}

// Tests that upon completing the batch upload flow, there are no separate
// profile and account sections.
- (void)testBatchUploadRemovesProfileSection {
  // Add one local bookmark.
  [BookmarkEarlGrey
      addBookmarkWithTitle:@"example1"
                       URL:@"https://www.example1.com"
                 inStorage:bookmarks::StorageType::kLocalOrSyncable];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Adds and signs in with `fakeIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Add one account bookmark.
  [BookmarkEarlGrey addBookmarkWithTitle:@"example2"
                                     URL:@"https://www.example2.com"
                               inStorage:bookmarks::StorageType::kAccount];
  [ChromeEarlGreyUI waitForAppToIdle];

  [BookmarkEarlGreyUI openBookmarks];

  NSString* profile_header =
      l10n_util::GetNSString(IDS_IOS_BOOKMARKS_PROFILE_SECTION_TITLE);
  NSString* account_header =
      l10n_util::GetNSString(IDS_IOS_BOOKMARKS_ACCOUNT_SECTION_TITLE);

  // Verify there exists separate profile and account sections.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(profile_header),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(account_header),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeBatchUploadButtonIdentifier)]
      performAction:grey_tap()];

  // Verify that alert is visible.
  ExpectBatchUploadAlert(1);

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertAction(l10n_util::GetNSString(
                     IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_ALERT_BUTTON))]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify only single "Mobile Bookmarks" is visible.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Verify that the profile and account headers no longer exist.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(profile_header),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(account_header),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

@end
