// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_matchers.h"

#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

id<GREYMatcher> FindEditMenuActionWithAccessibilityLabel(
    NSString* accessibility_label) {
  // The menu should be visible.
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()];

  NSError* error = nil;
  // First, check if the element is immediately visible
  id<GREYMatcher> edit_menu_item_matcher =
      grey_allOf([EditMenuAppInterface
                     editMenuActionWithAccessibilityLabel:accessibility_label],
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:edit_menu_item_matcher]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];
  if (!error) {
    return edit_menu_item_matcher;
  }
  error = nil;
  if (@available(iOS 26, *)) {
    // Tap on the next button
    [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface
                                            editMenuNextButtonMatcher]]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    if (error) {
      return nil;
    }
    [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface
                                            editMenuNextButtonMatcher]]
        performAction:grey_tap()];
    id<GREYMatcher> context_menu_item_matcher = grey_allOf(
        [EditMenuAppInterface
            contextMenuActionWithAccessibilityLabel:accessibility_label],
        grey_sufficientlyVisible(), nil);
    BOOL found = [ChromeEarlGrey
        testUIElementAppearanceWithMatcher:context_menu_item_matcher];
    return found ? context_menu_item_matcher : nil;
  }

  // Start on first screen (previous not visible or disabled).
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface
                                          editMenuPreviousButtonMatcher]]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)
                  error:&error];
  GREYAssert(error, @"FindEditMenuAction not called on the first page.");
  error = nil;
  [[[EarlGrey selectElementWithMatcher:edit_menu_item_matcher]
         usingSearchAction:grey_tap()
      onElementWithMatcher:[EditMenuAppInterface editMenuNextButtonMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];

  return error ? nil : edit_menu_item_matcher;
}
