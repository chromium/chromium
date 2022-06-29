// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/third_party/earl_grey2/src/CommonLib/Matcher/GREYLayoutConstraint.h"  // nogncheck

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests the Smart Sorting algorithm correctly sorts destinations in the new
// overflow menu carousel given certain usage.
@interface DestinationUsageHistoryCase : ChromeTestCase
@end

@implementation DestinationUsageHistoryCase

- (void)tearDown {
  // Close the overflow menu (popup menu).
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  [super tearDown];
}

#pragma mark - DestinationUsageHistoryCase Tests

// Tests the default sort order for the destinations carousel is correctly
// displayed. The default sort order is:
// 1. Bookmarks
// 2. History
// 3. Reading List
// 4. Password Manager
// 5. Downloads
// 6. Recent Tabs
// 7. Site Information
// 8. Settings
- (void)testDefaultCarouselSortOrderDisplayed {
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Unexpectedly, the first and last destinations in the carousel overlap their
  // neighbors. This makes `rightConstraint` an insufficient layout constraint
  // for comparing destinations at the carousel's ends. A constraint with
  // negative minimum separation, `rightConstraintWithOverlap`, must be
  // introduced to account for this.
  GREYLayoutConstraint* rightConstraintWithOverlap = [GREYLayoutConstraint
      layoutConstraintForDirection:kGREYLayoutDirectionRight
              andMinimumSeparation:-1.0];

  // . . . Bookmarks, History . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraintWithOverlap ],
                            chrome_test_util::BookmarksDestinationButton())];

  GREYLayoutConstraint* rightConstraint = [GREYLayoutConstraint
      layoutConstraintForDirection:kGREYLayoutDirectionRight
              andMinimumSeparation:0.0];

  // . . . History, Reading List . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ReadingListDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraint ],
                            chrome_test_util::HistoryDestinationButton())];
  // . . . Reading List, Password Manager . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::PasswordsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraint ],
                            chrome_test_util::ReadingListDestinationButton())];
  // . . . Password Manager, Downloads . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::DownloadsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraint ],
                            chrome_test_util::PasswordsDestinationButton())];
  // . . . Downloads, Recent Tabs . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::RecentTabsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraint ],
                            chrome_test_util::DownloadsDestinationButton())];
  // . . . Recent Tabs, Site Information . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SiteInfoDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraint ],
                            chrome_test_util::RecentTabsDestinationButton())];
  // . . . Site Information, Settings . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraintWithOverlap ],
                            chrome_test_util::SiteInfoDestinationButton())];
}

// Tests the default sort order for the destinations carousel is correctly
// displayed on the NTP. The default sort order is:
// 1. Bookmarks
// 2. History
// 3. Reading List
// 4. Password Manager
// 5. Downloads
// 6. Recent Tabs
// 7. Settings
// NOTE: By design, the Site Information destination is removed from the
// destinations carousel on the NTP.
- (void)testDefaultCarouselSortOrderDisplayedOnNTP {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Unexpectedly, the first and last destinations in the carousel overlap their
  // neighbors. This makes `rightConstraint` an insufficient layout constraint
  // for comparing destinations at the carousel's ends. A constraint with
  // negative minimum separation, `rightConstraintWithOverlap`, must be
  // introduced to account for this.
  GREYLayoutConstraint* rightConstraintWithOverlap = [GREYLayoutConstraint
      layoutConstraintForDirection:kGREYLayoutDirectionRight
              andMinimumSeparation:-1.0];

  // . . . Bookmarks, History . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraintWithOverlap ],
                            chrome_test_util::BookmarksDestinationButton())];

  GREYLayoutConstraint* rightConstraint = [GREYLayoutConstraint
      layoutConstraintForDirection:kGREYLayoutDirectionRight
              andMinimumSeparation:0.0];

  // . . . History, Reading List . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ReadingListDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraint ],
                            chrome_test_util::HistoryDestinationButton())];
  // . . . Reading List, Password Manager . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::PasswordsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraint ],
                            chrome_test_util::ReadingListDestinationButton())];
  // . . . Password Manager, Downloads . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::DownloadsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraint ],
                            chrome_test_util::PasswordsDestinationButton())];
  // . . . Downloads, Recent Tabs . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::RecentTabsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraint ],
                            chrome_test_util::DownloadsDestinationButton())];
  // . . . Recent Tabs, Settings . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ rightConstraintWithOverlap ],
                            chrome_test_util::RecentTabsDestinationButton())];
}

@end
