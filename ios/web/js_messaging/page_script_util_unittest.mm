// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/page_script_util.h"

#import <WebKit/WebKit.h>

#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/test/js_test_util_internal.h"
#import "testing/gtest_mac.h"

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

}  // namespace
}  // namespace web
