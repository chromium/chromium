// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/js_findinpage_manager.h"

#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture to test Find In Page JS.
class JsFindinpageManagerTest : public ChromeWebTest {
 protected:
  // Loads the given HTML and initializes the findInPage JS scripts.
  void LoadHtml(NSString* html) {
    ChromeWebTest::LoadHtml(html);
    manager_ =
        static_cast<JsFindinpageManager*>([web_state()->GetJSInjectionReceiver()
            instanceOfClass:[JsFindinpageManager class]]);
    [manager_ inject];
  }
  JsFindinpageManager* manager_;
};

// Tests that findString script reports a match when appropriate.
TEST_F(JsFindinpageManagerTest, FindInPageSucceeds) {
  LoadHtml(@"<html><body><p>Target phrase</p></body></html>");
  __block BOOL completion_handler_block_was_called = NO;
  id completion_handler_block = ^(BOOL success, CGPoint scrollPosition) {
    ASSERT_TRUE(success);
    // 'scrollPosition' is updated if the string is found.
    EXPECT_NE(FLT_MAX, scrollPosition.x);
    completion_handler_block_was_called = YES;
  };
  [manager_ findString:@"Target phrase"
      completionHandler:completion_handler_block];
  base::test::ios::WaitUntilCondition(^bool() {
    return completion_handler_block_was_called;
  });
}

// Tests that findString script does not report a match when appropriate.
TEST_F(JsFindinpageManagerTest, FindInPageFails) {
  LoadHtml(@"<html><body><p>Target phrase</p></body></html>");
  __block BOOL completion_handler_block_was_called = NO;
  id completion_handler_block = ^(BOOL success, CGPoint scrollPosition) {
    // findString should return YES even if the target phrase is not found.
    ASSERT_TRUE(success);
    // 'point' is *not* updated if the string is not found.
    EXPECT_TRUE(CGPointEqualToPoint(CGPointZero, scrollPosition));
    completion_handler_block_was_called = YES;
  };
  [manager_ findString:@"Non-included phrase"
      completionHandler:completion_handler_block];
  base::test::ios::WaitUntilCondition(^bool() {
    return completion_handler_block_was_called;
  });
}

// Attepting to break out of the script and inject new script fails.
TEST_F(JsFindinpageManagerTest, InjectionTest) {
  LoadHtml(@"<html><body><p>Target phrase</p></body></html>");
  __block BOOL completion_handler_block_was_called = NO;
  id completion_handler_block = ^(BOOL success, CGPoint scrollPosition) {
    [manager_ executeJavaScript:@"token"
              completionHandler:^(NSString* result, NSError*) {
                EXPECT_NSNE(@YES, result);
                completion_handler_block_was_called = YES;
              }];
  };
  [manager_ findString:@"');token=true;('"
      completionHandler:completion_handler_block];
  base::test::ios::WaitUntilCondition(^bool() {
    return completion_handler_block_was_called;
  });
}
