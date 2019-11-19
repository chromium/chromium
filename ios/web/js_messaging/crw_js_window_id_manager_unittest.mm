// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/crw_js_window_id_manager.h"

#import <WebKit/WebKit.h>

#import "ios/web/js_messaging/page_script_util.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Test fixture for testing CRWJSWindowIDManager class.
class JSWindowIDManagerTest : public PlatformTest {
 protected:
  TestBrowserState browser_state_;
};

// Tests that window ID injection by a second manager results in a different
// window ID.
TEST_F(JSWindowIDManagerTest, WindowIDDifferentManager) {
  // Inject the first manager.
  WKWebView* web_view = [[WKWebView alloc] init];
  test::ExecuteJavaScript(web_view,
                          GetDocumentStartScriptForAllFrames(&browser_state_));

  CRWJSWindowIDManager* manager =
      [[CRWJSWindowIDManager alloc] initWithWebView:web_view];
  [manager inject];
  EXPECT_NSEQ([manager windowID],
              test::ExecuteJavaScript(web_view, @"window.__gCrWeb.windowId"));

  // Inject the second manager.
  WKWebView* web_view2 = [[WKWebView alloc] init];
  test::ExecuteJavaScript(web_view2,
                          GetDocumentStartScriptForAllFrames(&browser_state_));

  CRWJSWindowIDManager* manager2 =
      [[CRWJSWindowIDManager alloc] initWithWebView:web_view2];
  [manager2 inject];
  EXPECT_NSEQ([manager2 windowID],
              test::ExecuteJavaScript(web_view2, @"window.__gCrWeb.windowId"));

  // Window IDs must be different.
  EXPECT_NSNE([manager windowID], [manager2 windowID]);
}

// Tests that injecting multiple times creates a new window ID.
TEST_F(JSWindowIDManagerTest, MultipleInjections) {
  WKWebView* web_view = [[WKWebView alloc] init];
  test::ExecuteJavaScript(web_view,
                          GetDocumentStartScriptForAllFrames(&browser_state_));

  // First injection.
  CRWJSWindowIDManager* manager =
      [[CRWJSWindowIDManager alloc] initWithWebView:web_view];
  [manager inject];
  NSString* windowID = [manager windowID];
  EXPECT_NSEQ(windowID,
              test::ExecuteJavaScript(web_view, @"window.__gCrWeb.windowId"));

  // Second injection.
  [manager inject];
  EXPECT_NSEQ([manager windowID],
              test::ExecuteJavaScript(web_view, @"window.__gCrWeb.windowId"));

  EXPECT_NSNE(windowID, [manager windowID]);
}

// Tests that injection will retry if |window.__gCrWeb| is not present.
TEST_F(JSWindowIDManagerTest, InjectionRetry) {
  WKWebView* web_view = [[WKWebView alloc] init];

  CRWJSWindowIDManager* manager =
      [[CRWJSWindowIDManager alloc] initWithWebView:web_view];
  [manager inject];
  EXPECT_TRUE([manager windowID]);
  EXPECT_FALSE(test::ExecuteJavaScript(web_view, @"window.__gCrWeb"));

  // Now inject window.__gCrWeb and check if window ID injection retried.
  test::ExecuteJavaScript(web_view,
                          GetDocumentStartScriptForAllFrames(&browser_state_));
  EXPECT_NSEQ([manager windowID],
              test::ExecuteJavaScript(web_view, @"window.__gCrWeb.windowId"));
}

}  // namespace web
