// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {

class PageStabilityJavascriptTest : public web::JavascriptTest {
 protected:
  void SetUp() override {
    web::JavascriptTest::SetUp();
    AddGCrWebScript();
    AddUserScript(@"page_stability");
    ASSERT_TRUE(LoadHtml(@"<html><body></body></html>"));
  }
};

TEST_F(PageStabilityJavascriptTest, WaitForStability_NoMutations_Resolves) {
  NSString* script = @R"(
    return __gCrWeb.getRegisteredApi('page_stability')
                   .getFunction('waitForStability')({
                     windowDurationMs: 100,
                     mutationCap: 5,
                     timeoutMs: 1000,
                   });
  )";
  id result = web::test::ExecuteAsyncJavaScript(web_view(), script, nil);
  NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
  ASSERT_TRUE(resultDict);
  EXPECT_TRUE([resultDict[@"settled"] boolValue]);
  EXPECT_EQ([resultDict[@"mutationCount"] intValue], 0);
}

TEST_F(PageStabilityJavascriptTest, WaitForStability_PageStabilizes_Resolves) {
  // Schedule a single mutation and wait for stability in a single block
  // to prevent scheduling race conditions.
  NSString* script = @R"(
    setTimeout(() => {
      document.body.appendChild(document.createElement('div'));
    }, 100);
    return __gCrWeb.getRegisteredApi('page_stability')
                   .getFunction('waitForStability')({
                     windowDurationMs: 300,
                     mutationCap: 5,
                     timeoutMs: 1000,
                   });
  )";

  id result = web::test::ExecuteAsyncJavaScript(web_view(), script, nil);
  NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
  ASSERT_TRUE(resultDict);
  EXPECT_TRUE([resultDict[@"settled"] boolValue]);
  EXPECT_EQ([resultDict[@"mutationCount"] intValue], 1);
}

TEST_F(PageStabilityJavascriptTest,
       WaitForStability_PageEventuallyStabilizes_ResolvesToSuccess) {
  // Schedule recurrent mutations to force the page to be unstable for the
  // first two windows, but stabilize during the third window.
  NSString* script = @R"(
    var count = 0;
    var interval = setInterval(() => {
      document.body.appendChild(document.createElement('div'));
      count++;
      if (count >= 8) clearInterval(interval);
    }, 30);
    return __gCrWeb.getRegisteredApi('page_stability')
                   .getFunction('waitForStability')({
                     windowDurationMs: 100,
                     mutationCap: 2,
                     timeoutMs: 2000,
                   });
  )";

  id result = web::test::ExecuteAsyncJavaScript(web_view(), script, nil);
  NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
  ASSERT_TRUE(resultDict);
  EXPECT_TRUE([resultDict[@"settled"] boolValue]);
  EXPECT_LE([resultDict[@"mutationCount"] intValue], 2);
}

TEST_F(PageStabilityJavascriptTest,
       WaitForStability_PageDoesNotStabilize_TimesOut) {
  // Schedule sustained infinite mutations to force the check to time out.
  NSString* script = @R"(
    var logs = [];
    window.startTime = performance.now();
    var startTime = window.startTime;
    var interval = setInterval(() => {
      document.body.appendChild(document.createElement('div'));
      logs.push('interval at ' + (performance.now() - startTime).toFixed(1) + 'ms');
    }, 30);
    var promise = __gCrWeb.getRegisteredApi('page_stability')
                         .getFunction('waitForStability')({
                           windowDurationMs: 100,
                           mutationCap: 2,
                           timeoutMs: 300,
                         });
    promise = promise.then((res) => {
      clearInterval(interval);
      res.logs = logs;
      res.debugLogs = window.debugLogs || [];
      res.elapsed = performance.now() - startTime;
      return res;
    }).catch((err) => {
      clearInterval(interval);
      throw err;
    });
    return promise;
  )";

  id result = web::test::ExecuteAsyncJavaScript(web_view(), script, nil);
  NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
  NSLog(@"DEBUG_RESULT: %@", resultDict);
  ASSERT_TRUE(resultDict);
  EXPECT_FALSE([resultDict[@"settled"] boolValue]);
  EXPECT_TRUE([resultDict[@"timedOut"] boolValue]);
}

TEST_F(PageStabilityJavascriptTest,
       WaitForStability_TimeoutLessThanWindow_TimesOut) {
  // Set timeout smaller than the window duration. It should time out
  // even if there are no mutations.
  NSString* script = @R"(
    return __gCrWeb.getRegisteredApi('page_stability')
                   .getFunction('waitForStability')({
                     windowDurationMs: 200,
                     mutationCap: 5,
                     timeoutMs: 50,
                   });
  )";
  id result = web::test::ExecuteAsyncJavaScript(web_view(), script, nil);
  NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
  ASSERT_TRUE(resultDict);
  EXPECT_FALSE([resultDict[@"settled"] boolValue]);
  EXPECT_TRUE([resultDict[@"timedOut"] boolValue]);
}

TEST_F(PageStabilityJavascriptTest,
       WaitForStability_ContinuousSingleMutationWithZeroCap_TimesOut) {
  // Schedule a mutation every 80ms. With a window duration of 100ms and cap of
  // 0, the page will never be stable, so it should time out.
  NSString* script = @R"(
    var interval = setInterval(() => {
      document.body.appendChild(document.createElement('div'));
    }, 80);
    var promise = __gCrWeb.getRegisteredApi('page_stability')
                         .getFunction('waitForStability')({
                           windowDurationMs: 100,
                           mutationCap: 0,
                           timeoutMs: 300,
                         });
    promise.then(() => { clearInterval(interval); })
           .catch(() => { clearInterval(interval); });
    return promise;
  )";
  id result = web::test::ExecuteAsyncJavaScript(web_view(), script, nil);
  NSDictionary* resultDict = base::apple::ObjCCast<NSDictionary>(result);
  ASSERT_TRUE(resultDict);
  EXPECT_FALSE([resultDict[@"settled"] boolValue]);
  EXPECT_TRUE([resultDict[@"timedOut"] boolValue]);
}

}  // namespace
