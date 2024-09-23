// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_scroll_observer.h"

#import "base/functional/bind.h"
#import "testing/platform_test.h"

namespace {

// Increments `counter`.
void IncrementCounter(size_t* counter) {
  ++*counter;
}

}  // anonymous namespace

using SessionRestorationScrollObserverTest = PlatformTest;

// Tests that the observer call the closure when the scroll event
// -webViewScrollViewDidEndDragging:willDecelerate: is called and
// that it stop calling the closure after -shutdown is called.
TEST_F(SessionRestorationScrollObserverTest, CallClosureOnDidEndDragging) {
  size_t call_count = 0;
  SessionRestorationScrollObserver* observer =
      [[SessionRestorationScrollObserver alloc]
          initWithClosure:base::BindRepeating(&IncrementCounter, &call_count)];

  // Check that there is no spurious calls during creation.
  ASSERT_EQ(call_count, 0u);

  // Check that each invocation is counted once.
  [observer webViewScrollViewDidEndDragging:nil willDecelerate:NO];
  [observer webViewScrollViewDidEndDragging:nil willDecelerate:NO];
  EXPECT_EQ(call_count, 2u);

  // Check that the closure is not called after shutdown.
  [observer shutdown];
  [observer webViewScrollViewDidEndDragging:nil willDecelerate:NO];
  EXPECT_EQ(call_count, 2u);
}

// Tests that the observer call the closure when the scroll event
// -webViewScrollViewDidEndZooming:atScale: is called and that it
// stop calling the closure after -shutdown is called.
TEST_F(SessionRestorationScrollObserverTest, CallClosureOnDidEndZooming) {
  size_t call_count = 0;
  SessionRestorationScrollObserver* observer =
      [[SessionRestorationScrollObserver alloc]
          initWithClosure:base::BindRepeating(&IncrementCounter, &call_count)];

  // Check that there is no spurious calls during creation.
  ASSERT_EQ(call_count, 0u);

  // Check that each invocation is counted once.
  [observer webViewScrollViewDidEndZooming:nil atScale:1.0];
  [observer webViewScrollViewDidEndZooming:nil atScale:1.0];
  EXPECT_EQ(call_count, 2u);

  // Check that the closure is not called after shutdown.
  [observer shutdown];
  [observer webViewScrollViewDidEndZooming:nil atScale:1.0];
  EXPECT_EQ(call_count, 2u);
}
