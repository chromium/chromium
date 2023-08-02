// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/signin/public/base/consent_level.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using chrome_test_util::BookmarksHomeDoneButton;
using chrome_test_util::BookmarksNavigationBarBackButton;
using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::OmniboxText;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

// Bookmark promo integration tests for Chrome with
// kEnableBookmarksAccountStorage enabled.
@interface BookmarksAccountStorageTestCase : WebHttpServerChromeTestCase
@end

@implementation BookmarksAccountStorageTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(syncer::kEnableBookmarksAccountStorage);
  return config;
}

- (void)setUp {
  [super setUp];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [ChromeEarlGrey clearBookmarks];
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  [ChromeEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

#pragma mark - BookmarksAccountStorageTestCase Tests

// Tests if there is only one "Mobile Bookmarks" in the bookmark list when
// the user is signed in+sync.
- (void)testMobileBookmarksWithSignInPlusSync {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  // Add the bookmark in the device storage.
  const GURL bookmarkURL = self.testServer->GetURL("/pony.html");
  std::string expectedURLContent = bookmarkURL.GetContent();
  NSString* bookmarkTitle = @"my bookmark";
  [ChromeEarlGrey loadURL:bookmarkURL];
  [[EarlGrey selectElementWithMatcher:OmniboxText(expectedURLContent)]
      assertWithMatcher:grey_notNil()];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGreyUI bookmarkCurrentTabWithTitle:bookmarkTitle];
  // Sign-in+sync with identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES];
  [BookmarkEarlGreyUI openBookmarks];
  // Tests that there is only one "Mobile Bookmarks".
  [[EarlGrey selectElementWithMatcher:grey_allOf(grey_kindOfClassName(
                                                     @"UITableViewCell"),
                                                 grey_descendant(grey_text(
                                                     @"Mobile Bookmarks")),
                                                 nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
