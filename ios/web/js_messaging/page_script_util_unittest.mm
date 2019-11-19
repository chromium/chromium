// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/page_script_util.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#include <memory>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/common/web_view_creation_util.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/test/web_test.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

// A test fixture for testing the page_script_util methods.
class PageScriptUtilTest : public WebTest {
 protected:
  PageScriptUtilTest() : WebTest(std::make_unique<TestWebClient>()) {}

  TestWebClient* GetWebClient() override {
    return static_cast<TestWebClient*>(WebTest::GetWebClient());
  }
};

// Tests that WKWebView early page script is a valid script that injects global
// __gCrWeb object.
TEST_F(PageScriptUtilTest, WKWebViewEarlyPageScript) {
  WKWebView* web_view = BuildWKWebView(CGRectZero, GetBrowserState());
  test::ExecuteJavaScript(
      web_view, GetDocumentStartScriptForAllFrames(GetBrowserState()));
  EXPECT_NSEQ(@"object", test::ExecuteJavaScript(web_view, @"typeof __gCrWeb"));
}

// Tests that embedder's WKWebView script is included into early script.
TEST_F(PageScriptUtilTest, WKEmbedderScript) {
  GetWebClient()->SetEarlyPageScript(@"__gCrEmbedder = {};");
  WKWebView* web_view = BuildWKWebView(CGRectZero, GetBrowserState());
  test::ExecuteJavaScript(
      web_view, GetDocumentStartScriptForAllFrames(GetBrowserState()));
  test::ExecuteJavaScript(
      web_view, GetDocumentStartScriptForMainFrame(GetBrowserState()));
  EXPECT_NSEQ(@"object",
              test::ExecuteJavaScript(web_view, @"typeof __gCrEmbedder"));
}

}  // namespace
}  // namespace web
