// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

namespace {

// Initiates a drag on the `link` element and drops it to the lower right of the
// web view.
bool LongPressLinkAndDragDropToWebView() {
  // Offset for dropping items onto the web view to avoid ending the drag within
  // the context menu bounds.
  CGVector kLowerRightWindowOffset = {0.85, 0.85};
  return chrome_test_util::LongPressLinkAndDragToView(
      @"link", /*src_window_number=*/0, /*dst_accessibility_identifier=*/nil,
      /*dst_window_number=*/0, kLowerRightWindowOffset);
}

}  // namespace

// Test case for drag and drop interactions in web content.
@interface WebDragDropTestCase : ChromeTestCase
@end

@implementation WebDragDropTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that dragging and dropping a web link opens the URL in a new tab.
- (void)testDragAndDropLink {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/link.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Link"];

  GREYAssert(LongPressLinkAndDragDropToWebView(),
             @"Failed to drag link to view");

  [ChromeEarlGrey waitForMainTabCount:2];
  const GURL expectedURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey waitForWebStateVisibleURL:expectedURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"Anyone know any good pony jokes?"];
}

// Tests that dragging and dropping a web link in an incognito tab opens the URL
// in a new incognito tab.
- (void)testDragAndDropLinkInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey closeAllNormalTabs];

  [ChromeEarlGrey waitForIncognitoTabCount:1];
  [ChromeEarlGrey waitForMainTabCount:0];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/link.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Link"];

  GREYAssert(LongPressLinkAndDragDropToWebView(),
             @"Failed to drag link to view");

  [ChromeEarlGrey waitForIncognitoTabCount:2];
  [ChromeEarlGrey waitForMainTabCount:0];
  const GURL expectedURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey waitForWebStateVisibleURL:expectedURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"Anyone know any good pony jokes?"];
}

// Tests that dragging and dropping a link with a disallowed scheme (e.g.
// javascript:) does not open a new tab or navigate.
- (void)testDragAndDropDisallowedSchemeDoesNotNavigate {
  const GURL initialURL = self.testServer->GetURL("/javascript_link.html");
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Link"];

  GREYAssert(LongPressLinkAndDragDropToWebView(),
             @"Failed to drag link to view");

  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForWebStateVisibleURL:initialURL];
}

// Tests that dragging and dropping a link embedded in an iframe opens the URL
// in a new tab.
- (void)testDragAndDropLinkFromIframe {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/iframe_drag_link.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Iframe Loaded"];

  GREYAssert(LongPressLinkAndDragDropToWebView(),
             @"Failed to drag link to view");

  [ChromeEarlGrey waitForMainTabCount:2];
  const GURL expectedURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey waitForWebStateVisibleURL:expectedURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"Anyone know any good pony jokes?"];
}

@end
