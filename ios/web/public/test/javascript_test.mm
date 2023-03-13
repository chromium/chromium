// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/javascript_test.h"

#import "ios/web/public/test/js_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

JavascriptTest::JavascriptTest() : web_view_([[WKWebView alloc] init]) {}
JavascriptTest::~JavascriptTest() {}

bool JavascriptTest::LoadHtml(NSString* html) {
  return web::test::LoadHtml(web_view_, html, nil);
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
      forMainFrameOnly:YES];
  [web_view_.configuration.userContentController addUserScript:script];
}

}  // namespace web