// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/eg_tests/composebox_egtest_util.h"

#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/tab_picker/ui/tab_picker_ui_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/tabs_egtest_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

void OpenComposeboxTabPicker() {
  // Focus the composebox.
  id<GREYMatcher> composebox_matcher =
      grey_accessibilityID(kComposeboxAccessibilityIdentifier);
  NSError* composebox_error = nil;
  [[EarlGrey selectElementWithMatcher:composebox_matcher]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&composebox_error];
  if (composebox_error) {
    NSError* fake_omnibox_error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
        performAction:grey_tap()
                error:&fake_omnibox_error];
    if (fake_omnibox_error) {
      [ChromeEarlGreyUI focusOmnibox];
    }

    // Wait for the composebox to be visible.
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:composebox_matcher];
  }

  // Tap the plus button.
  id<GREYMatcher> plus_button = grey_allOf(
      grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:plus_button];
  [[EarlGrey selectElementWithMatcher:plus_button] performAction:grey_tap()];

  // Tap the select tabs button.
  id<GREYMatcher> select_tabs_matcher = grey_allOf(
      grey_accessibilityID(kComposeboxSelectTabsActionAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:select_tabs_matcher];
  [[EarlGrey selectElementWithMatcher:select_tabs_matcher]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:select_tabs_matcher]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(
                          kTabPickerCollectionViewAccessibilityIdentifier)];
}

void SelectTabInTabPickerWithTitle(NSString* title) {
  id<GREYMatcher> tab_picker_matcher =
      grey_accessibilityID(kTabPickerCollectionViewAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:tab_picker_matcher];

  id<GREYMatcher> tab_matcher =
      grey_allOf(TabWithTitle(title), grey_minimumVisiblePercent(0.8), nil);
  [[[EarlGrey selectElementWithMatcher:tab_matcher]
         usingSearchAction:grey_scrollInDirectionWithStartPoint(
                               kGREYDirectionUp, /*amount=*/150,
                               /*xOriginStartPercentage=*/0.5,
                               /*yOriginStartPercentage=*/0.5)
      onElementWithMatcher:tab_picker_matcher] performAction:grey_tap()];
}
