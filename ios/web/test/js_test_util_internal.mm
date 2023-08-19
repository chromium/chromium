// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/js_test_util_internal.h"

#import <WebKit/WebKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "testing/gtest/include/gtest/gtest.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {
namespace test {

id ExecuteJavaScript(WKWebView* web_view,
                     WKContentWorld* content_world,
                     NSString* script) {
  __block id result;
  __block bool completed = false;
  __block NSError* block_error = nil;
  SCOPED_TRACE(base::SysNSStringToUTF8(script));
  [web_view evaluateJavaScript:script
                       inFrame:nil
                inContentWorld:content_world
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
  return result;
}

id ExecuteAsyncJavaScript(WKWebView* web_view,
                          WKContentWorld* content_world,
                          NSString* script) {
  __block id result;
  __block bool completed = false;
  __block NSError* block_error = nil;
  SCOPED_TRACE(base::SysNSStringToUTF8(script));
  [web_view callAsyncJavaScript:script
                      arguments:nil
                        inFrame:nil
                 inContentWorld:content_world
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
      << "\nWKWebView failed to complete async javascript execution.\n"
      << base::SysNSStringToUTF8(
             [[NSThread callStackSymbols] componentsJoinedByString:@"\n"]);
  return result;
}

}  // namespace test
}  // namespace web
