// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "ios/chrome/browser/pref_names.h"
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

namespace {
// Unexpectedly, the first and last destinations in the carousel overlap their
// neighbors. This makes |RightConstraint()| an insufficient layout constraint
// for comparing destinations at the carousel's ends. A constraint with
// negative minimum separation, |RightConstraintWithOverlap()|, must be
// introduced to account for this.
GREYLayoutConstraint* RightConstraintWithOverlap() {
  return [GREYLayoutConstraint
      layoutConstraintForDirection:kGREYLayoutDirectionRight
              andMinimumSeparation:-1.0];
}

GREYLayoutConstraint* RightConstraint() {
  return [GREYLayoutConstraint
      layoutConstraintForDirection:kGREYLayoutDirectionRight
              andMinimumSeparation:0.0];
}

}  // namespace

// Tests the Smart Sorting algorithm correctly sorts destinations in the new
// overflow menu carousel given certain usage.
@interface DestinationUsageHistoryCase : ChromeTestCase
@end

@implementation DestinationUsageHistoryCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kOverflowMenuDestinationUsageHistory];
}

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

  // . . . Bookmarks, History . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraintWithOverlap() ],
                            chrome_test_util::BookmarksDestinationButton())];

  // . . . History, Reading List . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ReadingListDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::HistoryDestinationButton())];
  // . . . Reading List, Password Manager . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::PasswordsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::ReadingListDestinationButton())];
  // . . . Password Manager, Downloads . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::DownloadsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::PasswordsDestinationButton())];
  // . . . Downloads, Recent Tabs . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::RecentTabsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::DownloadsDestinationButton())];
  // . . . Recent Tabs, Site Information . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SiteInfoDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::RecentTabsDestinationButton())];
  // . . . Site Information, Settings . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraintWithOverlap() ],
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

  // . . . Bookmarks, History . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraintWithOverlap() ],
                            chrome_test_util::BookmarksDestinationButton())];

  // . . . History, Reading List . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ReadingListDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::HistoryDestinationButton())];
  // . . . Reading List, Password Manager . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::PasswordsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::ReadingListDestinationButton())];
  // . . . Password Manager, Downloads . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::DownloadsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::PasswordsDestinationButton())];
  // . . . Downloads, Recent Tabs . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::RecentTabsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::DownloadsDestinationButton())];
  // . . . Recent Tabs, Settings . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraintWithOverlap() ],
                            chrome_test_util::RecentTabsDestinationButton())];
}

// For the following tests, it's important to note the destinations carousel is
// divided into two groups: (A) visible "above-the-fold" destinations and (B)
// non-visible "below-the-fold" destinations; "below-the-fold" destinations are
// made visible to the user when they scroll the carousel.

// Tests an above-the-fold destination never moves within group (A), regardless
// of usage.
- (void)testAboveFoldDestinationNeverPromotes {
  // Tap the above-fold destination, Password Manager, 5 times.
  for (int i = 0; i < 5; i++) {
    [ChromeEarlGreyUI openToolsMenu];
    [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
    [ChromeEarlGreyUI
        tapToolsMenuButton:chrome_test_util::PasswordsDestinationButton()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
        performAction:grey_tap()];
  }

  // Open the overflow menu to verify no changes to the carousel sort were made.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // . . . Bookmarks, History . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraintWithOverlap() ],
                            chrome_test_util::BookmarksDestinationButton())];

  // . . . History, Reading List . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ReadingListDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::HistoryDestinationButton())];
  // . . . Reading List, Password Manager . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::PasswordsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::ReadingListDestinationButton())];
  // . . . Password Manager, Downloads . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::DownloadsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::PasswordsDestinationButton())];
  // . . . Downloads, Recent Tabs . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::RecentTabsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::DownloadsDestinationButton())];
  // . . . Recent Tabs, Settings . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraintWithOverlap() ],
                            chrome_test_util::RecentTabsDestinationButton())];
}

// Tests a below-the-fold destination gets promoted.
- (void)testBelowFoldDestinationPromotes {
  // Tap the below-fold destination, Settings, 5 times.
  for (int i = 0; i < 5; i++) {
    [ChromeEarlGreyUI openToolsMenu];
    [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

    [ChromeEarlGreyUI
        tapToolsMenuButton:chrome_test_util::SettingsDestinationButton()];

    [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
        performAction:grey_tap()];
  }

  // Open the overflow menu to verify no changes to the carousel sort were made.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // . . . Settings, History . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraintWithOverlap() ],
                            chrome_test_util::SettingsDestinationButton())];

  // . . . History, Reading List . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ReadingListDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::HistoryDestinationButton())];
  // . . . Reading List, Password Manager . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::PasswordsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::ReadingListDestinationButton())];
  // . . . Password Manager, Downloads . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::DownloadsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::PasswordsDestinationButton())];
  // . . . Downloads, Recent Tabs . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::RecentTabsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::DownloadsDestinationButton())];
  // . . . Recent Tabs, Bookmarks . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::BookmarksDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraintWithOverlap() ],
                            chrome_test_util::RecentTabsDestinationButton())];
}

@end
