// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface TracingUITestCase : ChromeTestCase
@end

@implementation TracingUITestCase

// Tests that chrome://tracing loads correctly and that the UI can be
// interacted with to start and stop a trace.
- (void)testTracingUI {
  [ChromeEarlGrey loadURL:GURL("chrome://tracing/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Perfetto Tracing"];

  // Verify the "Start Recording" button is present and click it.
  [ChromeEarlGrey waitForWebStateContainingText:"Start Recording"];
  [ChromeEarlGrey tapWebStateElementWithID:@"start-button"];

  // Verify the UI changes state.
  [ChromeEarlGrey waitForWebStateContainingText:"Status: Recording"];

  // Click stop.
  [ChromeEarlGrey tapWebStateElementWithID:@"stop-button"];

  [ChromeEarlGrey verifyActivitySheetVisible];
  [ChromeEarlGrey closeActivitySheet];

  // Wait for the button state to revert.
  [ChromeEarlGrey waitForWebStateContainingText:"Status: Ready"];
}

@end
