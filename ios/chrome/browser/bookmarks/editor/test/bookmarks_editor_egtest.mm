// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/public/bookmarks_ui_constants.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::BookmarksSaveEditDoneButton;

// Test suite for the Bookmarks Editor.
@interface BookmarksEditorTestCase : WebHttpServerChromeTestCase
@end

@implementation BookmarksEditorTestCase

- (void)setUp {
  [super setUp];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey clearBookmarks];
}

// Tear down called once per test.
- (void)tearDownHelper {
  [super tearDownHelper];
  [BookmarkEarlGrey clearBookmarks];
}

// Tests that editing a bookmark after opening a URL from an external app
// doesn't crash Chrome. Regression test for crbug.com/475777100.
- (void)testEditBookmarkAfterOpeningExternalURL {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL bookmarkedURL = self.testServer->GetURL("/pony.html");

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [ChromeEarlGrey waitForWebStateVisibleURL:bookmarkedURL];

  [BookmarkEarlGreyUI starAndEditCurrentTabWithSnackbarTitle:nil];

  // Simulate opening a URL from an external app.
  const GURL externalURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:externalURL];

  // This checks that the editor can be opened again without causing Chrome to
  // crash.
  [BookmarkEarlGreyUI starAndEditCurrentTabWithSnackbarTitle:nil];
}

@end
