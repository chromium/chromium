// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/javascript_test.h"

#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/base/apple/url_conversions.h"

namespace web {

JavascriptTest::JavascriptTest() : web_view_([[WKWebView alloc] init]) {
  if (@available(iOS 16.4, *)) {
    web_view_.inspectable = YES;
  }
}
JavascriptTest::~JavascriptTest() {}

bool JavascriptTest::LoadHtml(NSString* html) {
  return web::test::LoadHtml(web_view_, html, nil);
}

bool JavascriptTest::LoadUrl(const GURL& url) {
  NSURLRequest* request =
      [[NSURLRequest alloc] initWithURL:net::NSURLWithGURL(url)];
  [web_view_ loadRequest:request];

  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return !web_view_.loading;
      });
}

void JavascriptTest::AddGCrWebScript() {
  AddUserScript(@"gcrweb");
}

void JavascriptTest::AddCommonScript() {
  AddUserScript(@"common");
}

void JavascriptTest::AddMessageScript() {
  AddUserScript(@"message");
}

void JavascriptTest::AddUserScript(NSString* script_name) {
  WKUserScript* script = [[WKUserScript alloc]
        initWithSource:web::test::GetPageScript(script_name)
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:NO];
  [web_view_.configuration.userContentController addUserScript:script];
}

}  // namespace web
