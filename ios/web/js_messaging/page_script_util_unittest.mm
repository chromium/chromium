// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/page_script_util.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/test/js_test_util_internal.h"
#import "testing/gtest_mac.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace {

NSString* GetSharedScripts() {
  // Scripts must be all injected at once because as soon as __gCrWeb exists,
  // injection is assumed to be done and __gCrWeb.message is used.
  return [NSString stringWithFormat:@"%@; %@; %@",
                                    web::test::GetPageScript(@"gcrweb"),
                                    web::test::GetPageScript(@"common"),
                                    web::test::GetPageScript(@"message")];
}

void AddSharedScriptsToWebView(WKWebView* web_view) {
  web::test::ExecuteJavaScript(web_view, GetSharedScripts());
}

}  // namespace

namespace web {
namespace {

// A test fixture for testing the page_script_util methods.
class PageScriptUtilTest : public WebTest {
 protected:
  PageScriptUtilTest() : WebTest(std::make_unique<FakeWebClient>()) {}

  FakeWebClient* GetWebClient() override {
    return static_cast<FakeWebClient*>(WebTest::GetWebClient());
  }
};

// Tests that `MakeScriptInjectableOnce` prevents a script from being injected
// twice.
TEST_F(PageScriptUtilTest, MakeScriptInjectableOnce) {
  WKWebView* web_view = BuildWKWebView(CGRectZero, GetBrowserState());
  NSString* identifier = @"script_id";

  test::ExecuteJavaScript(
      web_view, MakeScriptInjectableOnce(identifier, @"var value = 1;"));
  EXPECT_NSEQ(@(1), test::ExecuteJavaScript(web_view, @"value"));

  test::ExecuteJavaScript(web_view,
                          MakeScriptInjectableOnce(identifier, @"value = 2;"));
  EXPECT_NSEQ(@(1), test::ExecuteJavaScript(web_view, @"value"));
}

// Tests that WKWebView early page script is a valid script that injects global
// __gCrWeb object.
TEST_F(PageScriptUtilTest, WKWebViewEarlyPageScript) {
  WKWebView* web_view = BuildWKWebView(CGRectZero, GetBrowserState());
  AddSharedScriptsToWebView(web_view);
  test::ExecuteJavaScript(
      web_view, GetDocumentStartScriptForAllFrames(GetBrowserState()));
  EXPECT_NSEQ(@"object", test::ExecuteJavaScript(web_view, @"typeof __gCrWeb"));
}

// Tests that WKWebView shared scripts are valid scripts that injects global
// __gCrWeb object in an isolated world.
TEST_F(PageScriptUtilTest, WKWebViewEarlyPageScriptIsolatedWorld) {
  if (@available(iOS 14, *)) {
    WKWebView* web_view = BuildWKWebView(CGRectZero, GetBrowserState());
    WKContentWorld* content_world = WKContentWorld.defaultClientWorld;
    web::test::ExecuteJavaScript(web_view, content_world, GetSharedScripts());
    test::ExecuteJavaScript(
        web_view, content_world,
        GetDocumentStartScriptForAllFrames(GetBrowserState()));
    EXPECT_NSEQ(@"object", test::ExecuteJavaScript(web_view, content_world,
                                                   @"typeof __gCrWeb"));
  }
}

// Tests that embedder's WKWebView script is included into early script.
TEST_F(PageScriptUtilTest, WKEmbedderScript) {
  GetWebClient()->SetEarlyPageScriptForMainFrame(@"__gCrEmbedder = {};");
  WKWebView* web_view = BuildWKWebView(CGRectZero, GetBrowserState());
  AddSharedScriptsToWebView(web_view);
  test::ExecuteJavaScript(
      web_view, GetDocumentStartScriptForAllFrames(GetBrowserState()));
  test::ExecuteJavaScript(
      web_view, GetDocumentStartScriptForMainFrame(GetBrowserState()));
  EXPECT_NSEQ(@"object",
              test::ExecuteJavaScript(web_view, @"typeof __gCrEmbedder"));
}

}  // namespace
}  // namespace web
