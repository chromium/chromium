// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ios/web/public/test/element_selector.h"
#import "ios/web/shell/test/earl_grey/shell_actions.h"
#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"
#import "ios/web/shell/test/earl_grey/shell_matchers.h"
#import "ios/web/shell/test/earl_grey/web_shell_test_case.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using testing::ButtonWithAccessibilityLabel;
using testing::ElementToDismissAlert;

namespace {
const char kHtmlFile[] = "/context_menu.html";
}

// Context menu test cases for the web shell.
@interface ContextMenuTestCase : WebShellTestCase

@end

@implementation ContextMenuTestCase

// Tests context menu appears on a regular link.
// TODO(crbug.com/40896396): Test is flaky. Re-enable the test.
- (void)DISABLED_testContextMenu {
  const char linkID[] = "normal-link";
  NSString* const linkText = @"normal-link-text";
  const GURL pageURL = self.testServer->GetURL(kHtmlFile);

  [ShellEarlGrey loadURL:pageURL];
  [ShellEarlGrey waitForWebStateContainingText:linkText];

  [[EarlGrey selectElementWithMatcher:web::WebView()]
      performAction:web::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:linkID])];

  id<GREYMatcher> copyItem = ButtonWithAccessibilityLabel(@"Copy Link");
  id<GREYMatcher> cancelItem = ButtonWithAccessibilityLabel(@"Cancel");

  // Context menu should have a "copy link" item.
  [[EarlGrey selectElementWithMatcher:copyItem]
      assertWithMatcher:grey_notNil()];

  // Dismiss the context menu.
  [[EarlGrey selectElementWithMatcher:cancelItem] performAction:grey_tap()];

  // Wait for the context menu to be dismissed and check if it was.
  [ShellEarlGrey waitForUIElementToDisappearWithMatcher:copyItem];
}

// Tests context menu on element that has WebkitTouchCallout set to none from an
// ancestor and overridden.
// TODO(crbug.com/40896396): Test is flaky. Re-enable the test.
- (void)DISABLED_testContextMenuWebkitTouchCalloutOverride {
  const char linkID[] = "no-webkit-link";
  NSString* const linkText = @"no-webkit-link-text";
  const GURL pageURL = self.testServer->GetURL(kHtmlFile);

  [ShellEarlGrey loadURL:pageURL];
  [ShellEarlGrey waitForWebStateContainingText:linkText];

  [[EarlGrey selectElementWithMatcher:web::WebView()]
      performAction:web::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:linkID])];

  id<GREYMatcher> copyItem = ButtonWithAccessibilityLabel(@"Copy Link");
  id<GREYMatcher> cancelItem = ButtonWithAccessibilityLabel(@"Cancel");

  // Context menu should have a "copy link" item.
  [[EarlGrey selectElementWithMatcher:copyItem]
      assertWithMatcher:grey_notNil()];

  // Dismiss the context menu.
  [[EarlGrey selectElementWithMatcher:cancelItem] performAction:grey_tap()];

  // Wait for the context menu to be dismissed and check if it was.
  [ShellEarlGrey waitForUIElementToDisappearWithMatcher:copyItem];
}

@end
