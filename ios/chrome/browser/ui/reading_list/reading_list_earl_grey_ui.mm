// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_earl_grey_ui.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Redefine EarlGrey macro to use line number and file name taken from the place
// of ReadingListEarlGreyUIImpl macro instantiation, rather than local line
// number inside test helper method. Original EarlGrey macro definition also
// expands to EarlGreyImpl instantiation. [self earlGrey] is provided by a
// superclass and returns EarlGreyImpl object created with correct line number
// and filename.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#define EarlGrey [self earlGrey]
#pragma clang diagnostic pop

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

@implementation ReadingListEarlGreyUIImpl

// Opens the reading list menu.
- (void)openReadingList {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::ReadingListDestinationButton()];
  // It seems that sometimes there is a delay before the ReadingList is
  // displayed. See https://crbug.com/1109202 .
  GREYAssert(WaitUntilConditionOrTimeout(
                 kWaitForUIElementTimeout,
                 ^BOOL {
                   NSError* error = nil;
                   [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                                           kReadingListViewID)]
                       assertWithMatcher:grey_sufficientlyVisible()
                                   error:&error];
                   return error == nil;
                 }),
             @"Reading List didn't appear.");
}

@end
