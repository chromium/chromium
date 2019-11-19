// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/static_html_view_test_util.h"

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

namespace chrome_test_util {

namespace {
// Synchronously returns the result of executed JavaScript.
id ExecuteScriptInStaticController(
    StaticHtmlViewController* html_view_controller,
    NSString* script) {
  __block id result = nil;
  __block bool did_finish = false;
  [html_view_controller executeJavaScript:script
                        completionHandler:^(id script_result, NSError* error) {
                          result = [script_result copy];
                          did_finish = true;
                        }];

  // If a timeout is reached, then return |result|, which should be nil;
  bool completed = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return did_finish;
  });

  return completed ? result : nil;
}
}  // namespace

// Returns the StaticHtmlViewController for the given |web_state|. If none is
// found, it returns nil.
StaticHtmlViewController* GetStaticHtmlView(web::WebState* web_state) {
  // The WKWebView in a static HTML view isn't part of a
  // webState, but it does have the StaticHtmlViewController
  // as its navigation delegate. The WKWebView is the only child subview
  // of the web state's view.
  UIView* web_state_view = chrome_test_util::GetCurrentWebState()->GetView();
  WKWebView* web_view =
      base::mac::ObjCCast<WKWebView>(web_state_view.subviews.firstObject);
  return base::mac::ObjCCast<StaticHtmlViewController>(
      web_view.navigationDelegate);
}

bool StaticHtmlViewContainingText(web::WebState* web_state, std::string text) {
  StaticHtmlViewController* html_view_controller = GetStaticHtmlView(web_state);
  if (!html_view_controller) {
    return false;
  }

  id body = ExecuteScriptInStaticController(
      html_view_controller, @"document.body ? document.body.textContent : ''");
  return [body containsString:base::SysUTF8ToNSString(text)];
  return false;
}
}  // namespace chrome_test_util
