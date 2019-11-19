// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(CHROME_EARL_GREY_1)
#import <EarlGrey/EarlGrey.h>
#endif

#import <GTXiLib/GTXiLib.h>

#include "ios/chrome/test/earl_grey/accessibility_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

BOOL VerifyAccessibilityForCurrentScreen(NSError* error) {
  // TODO(crbug.com/972681): The GTX analytics ping is preventing the app from
  // idling, causing EG tests to fail.  Disabling analytics will allow tests to
  // run, but may not be the correct long-term solution.
  [GTXAnalytics setEnabled:NO];

  GTXToolKit* toolkit = [[GTXToolKit alloc] init];
  for (UIWindow* window in [[UIApplication sharedApplication] windows]) {
    // Run the checks on all elements on the screen.
    BOOL success = [toolkit checkAllElementsFromRootElements:@[ window ]
                                                       error:&error];
    if (!success || error) {
      return NO;
    }
  }
  return YES;
}

#if defined(CHROME_EARL_GREY_1)

void VerifyAccessibilityForCurrentScreen() {
  // TODO(crbug.com/972681): The GTX analytics ping is preventing the app from
  // idling, causing EG tests to fail.  Disabling analytics will allow tests to
  // run, but may not be the correct long-term solution.
  [GTXAnalytics setEnabled:NO];

  GTXToolKit* toolkit = [[GTXToolKit alloc] init];
  NSError* error = nil;
  for (UIWindow* window in [[UIApplication sharedApplication] windows]) {
    // Run the checks on all elements on the screen.
    BOOL success = [toolkit checkAllElementsFromRootElements:@[ window ]
                                                       error:&error];
    GREYAssert(success, @"Accessibility checks failed! Error: %@", error);
  }
}

#endif

}  // namespace chrome_test_util
