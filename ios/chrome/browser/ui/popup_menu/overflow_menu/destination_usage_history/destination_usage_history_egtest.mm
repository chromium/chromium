// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"

#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/third_party/earl_grey2/src/CommonLib/Matcher/GREYLayoutConstraint.h"  // nogncheck

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Unexpectedly, the first and last destinations in the carousel overlap their
// neighbors. This makes `RightConstraint()` an insufficient layout constraint
// for comparing destinations at the carousel's ends. A constraint with
// negative minimum separation, `RightConstraintWithOverlap()`, must be
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
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kOverflowMenuNewDestinations];
}

- (void)tearDown {
  // Close the overflow menu (popup menu).
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  [super tearDown];
}

#pragma mark - Helpers

// Verifies the destination carousel displays the default sort order, which is:
// 1. Bookmarks
// 2. History
// 3. Reading List
// 4. Password Manager
// 5. Downloads
// 6. Recent Tabs
// 7. Site Information
// 8. Settings
//
// When `isNTP` is true, this method excludes the Site Information (#7)
// destination from the layout check, because Site Information is excluded from
// the destinations carousel on the NTP.
+ (void)verifyCarouselHasDefaultSortOrderOnNTP:(BOOL)isNTP {
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

  if (isNTP) {
    // . . . Recent Tabs, Settings . . .
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::SettingsDestinationButton()]
        assertWithMatcher:grey_layout(
                              @[ RightConstraintWithOverlap() ],
                              chrome_test_util::RecentTabsDestinationButton())];
  } else {
    // Site Information is included in the destinations carousel on non-NTP
    // pages.

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

  [ChromeEarlGreyUI closeToolsMenu];
}

// Verifies the destination carousel displays the default sort order for
// incognito, which is:
// 1. Bookmarks
// 2. Reading List
// 3. Password Manager
// 4. Downloads
// 5. Site Information
// 6. Settings
//
// When `isNTP` is true, this method excludes the Site Information (#5)
// destination from the layout check, because Site Information is excluded from
// the destinations carousel on the NTP.
+ (void)verifyCarouselHasDefaultSortOrderOnNTPForIncognito:(BOOL)isNTP {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // . . . Bookmarks, Reading List . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ReadingListDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraintWithOverlap() ],
                            chrome_test_util::BookmarksDestinationButton())];
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

  if (isNTP) {
    // . . . Downloads, Settings . . .
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::SettingsDestinationButton()]
        assertWithMatcher:grey_layout(
                              @[ RightConstraintWithOverlap() ],
                              chrome_test_util::DownloadsDestinationButton())];
  } else {
    // Site Information is included in the destinations carousel on non-NTP
    // pages.

    // . . . Downloads, Site Information . . .
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::SiteInfoDestinationButton()]
        assertWithMatcher:grey_layout(
                              @[ RightConstraint() ],
                              chrome_test_util::DownloadsDestinationButton())];
    // . . . Site Information, Settings . . .
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::SettingsDestinationButton()]
        assertWithMatcher:grey_layout(
                              @[ RightConstraintWithOverlap() ],
                              chrome_test_util::SiteInfoDestinationButton())];
  }

  [ChromeEarlGreyUI closeToolsMenu];
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
  if (@available(iOS 15.0, *)) {
  } else {
    return;
  }

  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [DestinationUsageHistoryCase verifyCarouselHasDefaultSortOrderOnNTP:NO];
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
  if (@available(iOS 15.0, *)) {
  } else {
    return;
  }

  [DestinationUsageHistoryCase verifyCarouselHasDefaultSortOrderOnNTP:YES];
}

// Tests the default sort order for the destinations carousel is correctly
// displayed for an incognito page. The default sort order is:
// 1. Bookmarks
// 2. Reading List
// 3. Password Manager
// 4. Downloads
// 5. Site Information
// 6. Settings
- (void)testDefaultCarouselSortOrderDisplayedForIncognito {
  if (@available(iOS 15.0, *)) {
  } else {
    return;
  }

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [DestinationUsageHistoryCase
      verifyCarouselHasDefaultSortOrderOnNTPForIncognito:NO];
}

// Tests the default sort order for the destinations carousel is correctly
// displayed on the incognito NTP. The default sort order is:
// 1. Bookmarks
// 2. Reading List
// 3. Password Manager
// 4. Downloads
// 5. Settings
//
// NOTE: By design, the Site Information destination is removed from the
// destinations carousel on the NTP.
- (void)testDefaultCarouselSortOrderDisplayedOnNTPForIncognito {
  if (@available(iOS 15.0, *)) {
  } else {
    return;
  }

  [ChromeEarlGrey openNewIncognitoTab];
  [DestinationUsageHistoryCase
      verifyCarouselHasDefaultSortOrderOnNTPForIncognito:YES];
}

// For the following tests, it's important to note the destinations carousel is
// divided into two groups: (A) visible "above-the-fold" destinations and (B)
// non-visible "below-the-fold" destinations; "below-the-fold" destinations are
// made visible to the user when they scroll the carousel.

// Tests an above-the-fold destination never moves within group (A),
// regardless of usage.
- (void)testAboveFoldDestinationNeverPromotes {
  if (@available(iOS 15.0, *)) {
  } else {
    return;
  }

  // Tap the above-fold destination, Password Manager, 5 times.
  for (int i = 0; i < 5; i++) {
    [ChromeEarlGreyUI openToolsMenu];
    [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
    [ChromeEarlGreyUI
        tapToolsMenuButton:chrome_test_util::PasswordsDestinationButton()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
        performAction:grey_tap()];
  }

  [DestinationUsageHistoryCase verifyCarouselHasDefaultSortOrderOnNTP:YES];
}

// Tests a below-the-fold destination gets promoted.
- (void)testBelowFoldDestinationPromotes {
  if (@available(iOS 15.0, *)) {
  } else {
    return;
  }

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

// Tests a below-the-fold destination is not promoted until the third click
// for a fresh destination usage history.
- (void)testNoSwapUntilMinClickCountReached {
  if (@available(iOS 15.0, *)) {
  } else {
    return;
  }

  [DestinationUsageHistoryCase verifyCarouselHasDefaultSortOrderOnNTP:YES];

  // 1st Settings tap (no promotion expected after this tap)
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::SettingsDestinationButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  [DestinationUsageHistoryCase verifyCarouselHasDefaultSortOrderOnNTP:YES];

  // 2nd Settings tap (no promotion expected after this tap)
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::SettingsDestinationButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  [DestinationUsageHistoryCase verifyCarouselHasDefaultSortOrderOnNTP:YES];

  // 3rd Settings tap (promotion expected after this tap!)
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::SettingsDestinationButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

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

// Tests the last immediately visible carousel destination can be promoted.
//
// When the new overflow menu is opened, the last immediately visible
// destination should be part of group (B)—the "below-the-fold" destinations
// list. This test verifies that this destination is correctly part of group (B)
// initially, and, with enough usage, is promoted to group (A)—the
// "above-the-fold" destinations.
- (void)testLastImmediatelyVisibleDestinationPromotes {
  if (@available(iOS 15.0, *)) {
  } else {
    return;
  }

  [DestinationUsageHistoryCase verifyCarouselHasDefaultSortOrderOnNTP:YES];

  // 1st Downloads tap (no promotion expected after this tap)
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::DownloadsDestinationButton()];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  [DestinationUsageHistoryCase verifyCarouselHasDefaultSortOrderOnNTP:YES];

  // 2nd Downloads tap (no promotion expected after this tap)
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::DownloadsDestinationButton()];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  [DestinationUsageHistoryCase verifyCarouselHasDefaultSortOrderOnNTP:YES];

  // 3rd Downloads tap (promotion expected after this tap!)
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::DownloadsDestinationButton()];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // . . . Downloads, History . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraintWithOverlap() ],
                            chrome_test_util::DownloadsDestinationButton())];

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
      selectElementWithMatcher:chrome_test_util::BookmarksDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::PasswordsDestinationButton())];
  // . . . Bookmarks, Recent Tabs . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::RecentTabsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraint() ],
                            chrome_test_util::BookmarksDestinationButton())];
  // . . . Recent Tabs, Bookmarks . . .
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsDestinationButton()]
      assertWithMatcher:grey_layout(
                            @[ RightConstraintWithOverlap() ],
                            chrome_test_util::RecentTabsDestinationButton())];
}

@end
