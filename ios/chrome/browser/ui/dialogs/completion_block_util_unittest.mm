// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/completion_block_util.h"

#include "base/bind.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using SafeWebCompletionTest = PlatformTest;
using completion_block_util::AlertCallback;
using completion_block_util::ConfirmCallback;
using completion_block_util::PromptCallback;
using completion_block_util::HTTPAuthCallack;
using completion_block_util::DecidePolicyCallback;
using completion_block_util::GetSafeJavaScriptAlertCompletion;
using completion_block_util::GetSafeJavaScriptConfirmationCompletion;
using completion_block_util::GetSafeJavaScriptPromptCompletion;
using completion_block_util::GetSafeHTTPAuthCompletion;
using completion_block_util::GetSafeDecidePolicyCompletion;

// Tests that a safe JavaScript alert completion block executes the original
// callback if deallocated.
TEST_F(SafeWebCompletionTest, JavaScriptAlert) {
  __block BOOL callback_executed = NO;
  @autoreleasepool {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
    AlertCallback callback = GetSafeJavaScriptAlertCompletion(^(void) {
      callback_executed = YES;
    });
#pragma clang diagnostic pop
  }
  EXPECT_TRUE(callback_executed);
}

// Tests that a safe JavaScript confirmation completion block executes the
// original callback if deallocated.
TEST_F(SafeWebCompletionTest, JavaScriptConfirmation) {
  __block BOOL callback_executed = NO;
  @autoreleasepool {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
    ConfirmCallback callback =
        GetSafeJavaScriptConfirmationCompletion(^(bool confirmed) {
          EXPECT_FALSE(confirmed);
          callback_executed = YES;
        });
#pragma clang diagnostic pop
  }
  EXPECT_TRUE(callback_executed);
}

// Tests that a safe JavaScript prompt completion block executes the original
// callback if deallocated.
TEST_F(SafeWebCompletionTest, JavaScriptPrompt) {
  __block BOOL callback_executed = NO;
  @autoreleasepool {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
    PromptCallback callback =
        GetSafeJavaScriptPromptCompletion(^(NSString* input) {
          EXPECT_FALSE(input);
          callback_executed = YES;
        });
#pragma clang diagnostic pop
  }
  EXPECT_TRUE(callback_executed);
}

// Tests that a safe HTTP authentication completion block executes the original
// callback if deallocated.
TEST_F(SafeWebCompletionTest, HTTPAuth) {
  __block BOOL callback_executed = NO;
  @autoreleasepool {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
    HTTPAuthCallack callback =
        GetSafeHTTPAuthCompletion(^(NSString* user, NSString* password) {
          EXPECT_FALSE(user);
          EXPECT_FALSE(password);
          callback_executed = YES;
        });
#pragma clang diagnostic pop
  }
  EXPECT_TRUE(callback_executed);
}

// Tests that a safe decide policy completion block executes the original
// callback if deallocated.
TEST_F(SafeWebCompletionTest, DecidePolicy) {
  __block BOOL callback_executed = NO;
  @autoreleasepool {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
    DecidePolicyCallback callback =
        GetSafeDecidePolicyCompletion(^(bool shouldContinue) {
          EXPECT_FALSE(shouldContinue);
          callback_executed = YES;
        });
#pragma clang diagnostic pop
  }
  EXPECT_TRUE(callback_executed);
}
