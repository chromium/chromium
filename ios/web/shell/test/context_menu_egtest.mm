// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/block_types.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/shell/test/earl_grey/shell_actions.h"
#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"
#import "ios/web/shell/test/earl_grey/shell_matchers.h"
#import "ios/web/shell/test/earl_grey/web_shell_test_case.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::ButtonWithAccessibilityLabel;
using testing::ElementToDismissAlert;

namespace {
const char kHtmlFile[] =
    "/ios/testing/data/http_server_files/context_menu.html";
}

// Context menu test cases for the web shell.
@interface ContextMenuTestCase : WebShellTestCase {
  net::EmbeddedTestServer _server;
}

@end

@implementation ContextMenuTestCase

- (void)setUp {
  [super setUp];

  NSString* bundlePath = [NSBundle bundleForClass:[self class]].resourcePath;
  _server.ServeFilesFromDirectory(
      base::FilePath(base::SysNSStringToUTF8(bundlePath)));
  GREYAssert(_server.Start(), @"EmbeddedTestServer failed to start.");
}

// Tests context menu appears on a regular link.
- (void)testContextMenu {
  const char linkID[] = "normal-link";
  NSString* const linkText = @"normal-link-text";
  const GURL pageURL = _server.GetURL(kHtmlFile);

  [ShellEarlGrey loadURL:pageURL];
  [ShellEarlGrey waitForWebStateContainingText:linkText];

  [[EarlGrey selectElementWithMatcher:web::WebView()]
      performAction:web::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:linkID])];

  id<GREYMatcher> copyItem = ButtonWithAccessibilityLabel(@"Copy Link");

  // Context menu should have a "copy link" item.
  [[EarlGrey selectElementWithMatcher:copyItem]
      assertWithMatcher:grey_notNil()];

  // Dismiss the context menu.
  [[EarlGrey selectElementWithMatcher:ElementToDismissAlert(@"Cancel")]
      performAction:grey_tap()];

  // Context menu should go away after the tap.
  [[EarlGrey selectElementWithMatcher:copyItem] assertWithMatcher:grey_nil()];
}

// Tests context menu on element that has WebkitTouchCallout set to none from an
// ancestor and overridden.
- (void)testContextMenuWebkitTouchCalloutOverride {
  const char linkID[] = "no-webkit-link";
  NSString* const linkText = @"no-webkit-link-text";
  const GURL pageURL = _server.GetURL(kHtmlFile);

  [ShellEarlGrey loadURL:pageURL];
  [ShellEarlGrey waitForWebStateContainingText:linkText];

  [[EarlGrey selectElementWithMatcher:web::WebView()]
      performAction:web::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:linkID])];

  id<GREYMatcher> copyItem = ButtonWithAccessibilityLabel(@"Copy Link");

  // Context menu should have a "copy link" item.
  [[EarlGrey selectElementWithMatcher:copyItem]
      assertWithMatcher:grey_notNil()];

  // Dismiss the context menu.
  [[EarlGrey selectElementWithMatcher:ElementToDismissAlert(@"Cancel")]
      performAction:grey_tap()];

  // Context menu should go away after the tap.
  [[EarlGrey selectElementWithMatcher:copyItem] assertWithMatcher:grey_nil()];
}

@end
