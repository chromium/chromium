// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_scroll_observer.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class GeminiScrollObserverTest : public PlatformTest {
 protected:
  // Sets up the test fixture by initializing the observer with a callback.
  void SetUp() override {
    PlatformTest::SetUp();
    callback_called_ = false;
    observer_ = [[GeminiScrollObserver alloc]
        initWithScrollCallback:base::BindRepeating(
                                   &GeminiScrollObserverTest::OnCallback,
                                   base::Unretained(this))];
    ASSERT_NE(observer_, nil);
  }

  // Cleans up the observer.
  void TearDown() override {
    observer_ = nil;
    PlatformTest::TearDown();
  }

  // Callback method that sets the flag when executed.
  void OnCallback() { callback_called_ = true; }

  GeminiScrollObserver* observer_;
  bool callback_called_;
};

TEST_F(GeminiScrollObserverTest, TestScrollCallbackExecuted) {
  EXPECT_FALSE(callback_called_);

  // Simulate scroll event by calling the observer method directly.
  // We pass nil as the proxy since it is not used in the implementation.
  [observer_ webViewScrollViewWillBeginDragging:nil];

  EXPECT_TRUE(callback_called_);
}

TEST_F(GeminiScrollObserverTest, TestScrollCallbackMultipleTimes) {
  EXPECT_FALSE(callback_called_);

  [observer_ webViewScrollViewWillBeginDragging:nil];
  EXPECT_TRUE(callback_called_);

  callback_called_ = false;

  [observer_ webViewScrollViewWillBeginDragging:nil];
  EXPECT_TRUE(callback_called_);
}

TEST_F(GeminiScrollObserverTest, TestNullCallbackDoesNotCrash) {
  GeminiScrollObserver* null_callback_observer = [[GeminiScrollObserver alloc]
      initWithScrollCallback:base::RepeatingClosure()];

  // This should not crash even though there is no callback to run.
  [null_callback_observer webViewScrollViewWillBeginDragging:nil];
}

}  // namespace
