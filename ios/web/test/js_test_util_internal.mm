// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/js_test_util_internal.h"

#import <WebKit/WebKit.h>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/common/features.h"
#import "testing/gtest/include/gtest/gtest.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {
namespace test {

namespace {

void ExecuteJavaScript(WKWebView* web_view,
                       WKContentWorld* content_world,
                       NSString* script,
                       id __autoreleasing* result) {
  __block id block_result;
  __block bool completed = false;
  __block NSError* block_error = nil;
  SCOPED_TRACE(base::SysNSStringToUTF8(script));
  [web_view evaluateJavaScript:script
                       inFrame:nil
                inContentWorld:content_world
             completionHandler:^(id script_result, NSError* script_error) {
               block_result = [script_result copy];
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

  if (block_error) {
    DLOG(WARNING) << "\nWKWebView javascript execution failed.\n"
                  << base::SysNSStringToUTF8(block_error.description);

    BOOL unsupportedResultError =
        [block_error.domain isEqualToString:WKErrorDomain] &&
        block_error.code == WKErrorJavaScriptResultTypeIsUnsupported;
    if (  // Caller cares about the result but it was of an unsupported type.
        (result && unsupportedResultError) ||
        // Error is a real failure
        !unsupportedResultError) {
      DLOG(WARNING) << "\nWKWebView javascript execution failed.\n"
                    << base::SysNSStringToUTF8(block_error.description);

      if (base::FeatureList::IsEnabled(
              web::features::kAssertOnJavaScriptErrors)) {
        CHECK(false) << "JavaScript error occurred with "
                        "kAssertOnJavaScriptErrors enabled.";
      }
    }
  }

  if (result) {
    *result = block_result;
  }
}

}  // namespace

void ExecuteJavaScriptInWebViewAndWorld(WKWebView* web_view,
                                        WKContentWorld* content_world,
                                        NSString* script) {
  ExecuteJavaScript(web_view, content_world, script, nil);
}

id ExecuteJavaScript(WKWebView* web_view,
                     WKContentWorld* content_world,
                     NSString* script) {
  id result;
  ExecuteJavaScript(web_view, content_world, script, &result);
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
