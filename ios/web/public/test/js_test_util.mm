// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/js_test_util.h"

#import <WebKit/WebKit.h>

#import "base/mac/bundle_locations.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/page_script_util.h"
#import "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {
namespace test {

id ExecuteJavaScript(WKWebView* web_view, NSString* script) {
  return ExecuteJavaScript(web_view, script, /*error=*/nil);
}

id ExecuteJavaScript(WKWebView* web_view,
                     NSString* script,
                     NSError* __autoreleasing* error) {
  __block id result;
  __block bool completed = false;
  __block NSError* block_error = nil;
  SCOPED_TRACE(base::SysNSStringToUTF8(script));
  [web_view evaluateJavaScript:script
             completionHandler:^(id script_result, NSError* script_error) {
               result = [script_result copy];
               block_error = [script_error copy];
               completed = true;
             }];
  BOOL success = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return completed;
  });
  // Log stack trace to provide some context.
  EXPECT_TRUE(success)
      << base::SysNSStringToUTF8(block_error.description)
      << "\nWKWebView failed to complete javascript execution.\n"
      << base::SysNSStringToUTF8(
             [[NSThread callStackSymbols] componentsJoinedByString:@"\n"]);
  if (error) {
    *error = block_error;
  }
  return result;
}

bool LoadHtml(WKWebView* web_view, NSString* html, NSURL* base_url) {
  [web_view loadHTMLString:html baseURL:base_url];

  return WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_view.loading;
  });
}

bool WaitForInjectedScripts(WKWebView* web_view) {
  return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !![ExecuteJavaScript(web_view, @"!!__gCrWeb") isEqual:@YES];
  });
}

NSString* GetPageScript(NSString* script_file_name) {
  return web::GetPageScript(script_file_name);
}

NSString* GetSharedScripts() {
  // Scripts must be all injected at once because as soon as __gCrWeb exists,
  // injection is assumed to be done and __gCrWeb.message is used.
  return [NSString stringWithFormat:@"%@; %@; %@", GetPageScript(@"gcrweb"),
                                    GetPageScript(@"common"),
                                    GetPageScript(@"message")];
}

}  // namespace test
}  // namespace web

