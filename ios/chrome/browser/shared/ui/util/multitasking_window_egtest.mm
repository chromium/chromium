// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_multitasking_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// Returns YES if the test should be skipped (e.g. not on iPad or iOS 26+).
BOOL ShouldSkipTest() {
  return ![ChromeEarlGrey isIPadIdiom] || !base::ios::IsRunningOnIOS26OrLater();
}

}  // namespace

// Tests multitasking window features, including entering/exiting Stage Manager
// mode and resizing the application window between regular and compact sizes.
@interface MultitaskingWindowTestCase : ChromeTestCase
@end

@implementation MultitaskingWindowTestCase

- (void)setUp {
  [super setUp];
  if ([ChromeEarlGrey isWindowedMode]) {
    [ChromeMultitaskingTestUtil moveToFullscreenMode];
  }
  GREYAssertFalse(
      [ChromeEarlGrey isWindowedMode],
      @"Test did not start cleanly with the app across fullscreen.");
}

- (void)tearDownHelper {
  if ([ChromeEarlGrey isWindowedMode]) {
    [ChromeMultitaskingTestUtil moveToFullscreenMode];
  }
  GREYAssertFalse([ChromeEarlGrey isWindowedMode],
                  @"Test did not end with the app running in fullscreen.");
  [super tearDownHelper];
}

// Tests that tapping the status bar switches the app to windowed mode,
// dragging the bottom-left corner resizes it to compact size class,
// and dragging it back restores regular size class.
- (void)testStatusBarTapToWindowedModeAndResize {
  if (ShouldSkipTest()) {
    EARL_GREY_TEST_SKIPPED(
        @"Multitasking tests are only supported on iPad and iOS 26+.");
  }

  [ChromeMultitaskingTestUtil moveToWindowedMode];
  [ChromeMultitaskingTestUtil resizeWindowToCompact];
  [ChromeMultitaskingTestUtil resizeWindowToRegular];
  [ChromeMultitaskingTestUtil moveToFullscreenMode];
}

// Tests that tapping the status bar switches the app to windowed mode,
// and selecting Full Screen from the multitasking menu returns it to
// fullscreen.
- (void)testStatusBarTapToWindowedModeAndReturnToFullscreen {
  if (ShouldSkipTest()) {
    EARL_GREY_TEST_SKIPPED(
        @"Multitasking tests are only supported on iPad and iOS 26+.");
  }

  [ChromeMultitaskingTestUtil moveToWindowedMode];
  [ChromeMultitaskingTestUtil moveToFullscreenMode];
}

// Tests that the app window can be transitioned to compact width and
// then restored directly to fullscreen mode from the compact layout state.
- (void)testResizeToCompactAndReturnToFullscreen {
  if (ShouldSkipTest()) {
    EARL_GREY_TEST_SKIPPED(
        @"Multitasking tests are only supported on iPad and iOS 26+.");
  }

  [ChromeMultitaskingTestUtil moveToWindowedMode];
  [ChromeMultitaskingTestUtil resizeWindowToCompact];
  [ChromeMultitaskingTestUtil moveToFullscreenMode];
}

// Tests that the app window can be transitioned multiple times between compact
// and regular size classes and then successfully restored to fullscreen.
- (void)testStatusBarTapToWindowedModeAndMultipleResizes {
  if (ShouldSkipTest()) {
    EARL_GREY_TEST_SKIPPED(
        @"Multitasking tests are only supported on iPad and iOS 26+.");
  }

  [ChromeMultitaskingTestUtil moveToWindowedMode];
  [ChromeMultitaskingTestUtil resizeWindowToCompact];
  [ChromeMultitaskingTestUtil resizeWindowToRegular];
  [ChromeMultitaskingTestUtil resizeWindowToCompact];
  [ChromeMultitaskingTestUtil resizeWindowToRegular];
  [ChromeMultitaskingTestUtil moveToFullscreenMode];
}

@end
