// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/activity_overlay_egtest_util.h"

#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

void WaitForActivityOverlayToDisappear() {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:
                   grey_allOf(grey_accessibilityID(
                                  kActivityOverlayViewAccessibilityIdentifier),
                              grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error != nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout, condition),
             @"Waiting for the overlay to disappear");
}
