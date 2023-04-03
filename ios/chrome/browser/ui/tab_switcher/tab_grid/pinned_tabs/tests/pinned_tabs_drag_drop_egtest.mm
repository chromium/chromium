// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::base::test::ios::kWaitForUIElementTimeout;
using ::base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::LongPressCellAndDragToOffsetOf;

namespace {

// Identifer for cell at given `index` in the tab grid.
NSString* IdentifierForRegularCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u", kGridCellIdentifierPrefix, index];
}

// Identifer for cell at given `index` in the pinned view.
NSString* IdentifierForPinnedCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u", kPinnedCellIdentifier, index];
}

// Matcher for the regual cell at the given `index`.
id<GREYMatcher> RegularCellAtIndex(unsigned int index) {
  return grey_allOf(
      grey_accessibilityID(IdentifierForRegularCellAtIndex(index)),
      grey_kindOfClassName(@"GridCell"), grey_sufficientlyVisible(), nil);
}

// Matcher for the pinned cell at the given `index`.
id<GREYMatcher> PinnedCellAtIndex(unsigned int index) {
  return grey_allOf(grey_accessibilityID(IdentifierForPinnedCellAtIndex(index)),
                    grey_kindOfClassName(@"PinnedCell"),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the pinned view.
id<GREYMatcher> PinnedView() {
  return grey_allOf(grey_accessibilityID(kPinnedViewIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Finds the element with the given `identifier` of given `type`.
XCUIElement* GetElementMatchingIdentifier(XCUIApplication* app,
                                          NSString* identifier,
                                          XCUIElementType type) {
  XCUIElementQuery* query = [[app.windows.firstMatch
      descendantsMatchingType:type] matchingIdentifier:identifier];
  return [query elementBoundByIndex:0];
}

// Drags and drops the cell with the given `cell_identifier` in the pinned view.
void DragDropCellInPinnedView(NSString* cell_identifier) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* src_element =
      GetElementMatchingIdentifier(app, cell_identifier, XCUIElementTypeCell);
  XCUICoordinate* start_point =
      [src_element coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];

  // Supposed position of the pinned view.
  // The pinned view is hidden when there is no pinned tabs. We can't determine
  // its position precisely.
  XCUICoordinate* end_point =
      [app coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.82)];

  [start_point pressForDuration:1.5
           thenDragToCoordinate:end_point
                   withVelocity:XCUIGestureVelocityDefault
            thenHoldForDuration:1.0];
}

// Checks that the regular cell at `regular_index` index has been moved to the
// pinned view at `pinned_index` index .
void AssertRegularCellMovedToPinnedView(unsigned int regular_index,
                                        unsigned int pinned_index) {
  ConditionBlock condition = ^{
    NSError* error1 = nil;
    NSError* error2 = nil;

    [[EarlGrey selectElementWithMatcher:RegularCellAtIndex(regular_index)]
        assertWithMatcher:grey_nil()
                    error:&error1];
    [[EarlGrey selectElementWithMatcher:PinnedCellAtIndex(pinned_index)]
        assertWithMatcher:grey_notNil()
                    error:&error2];

    return !error1 && !error2;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"The drag drop action has failed.");
}

}  // namespace

// Tests Drag and Drop interactions for the Pinned Tabs feature.
@interface PinnedTabsDragDropTestCase : ChromeTestCase
@end

@implementation PinnedTabsDragDropTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kEnablePinnedTabs);

  return config;
}

// Checks that dragging a regular tab and dropping it in the pinned view moves
// it in the pinned view.
- (void)testDragRegularTabInPinnedView {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  [ChromeEarlGreyUI openTabGrid];

  // The pinned view should not be visible when there is no pinned tabs.
  [[EarlGrey selectElementWithMatcher:PinnedView()]
      assertWithMatcher:grey_notVisible()];

  [[EarlGrey selectElementWithMatcher:RegularCellAtIndex(0)]
      assertWithMatcher:grey_notNil()];

  // Drag the first cell in the pinned view.
  DragDropCellInPinnedView(IdentifierForRegularCellAtIndex(0));
  AssertRegularCellMovedToPinnedView(/*regular_index*/ 0, /*pinned_index*/ 0);
  [[EarlGrey selectElementWithMatcher:PinnedView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open 2 new tabs.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];

  // Drag the second cell in the pinned view.
  DragDropCellInPinnedView(IdentifierForRegularCellAtIndex(1));
  AssertRegularCellMovedToPinnedView(/*regular_index*/ 1, /*pinned_index*/ 1);
  [[EarlGrey selectElementWithMatcher:PinnedView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Drag the last cell in the pinned view.
  DragDropCellInPinnedView(IdentifierForRegularCellAtIndex(0));
  AssertRegularCellMovedToPinnedView(/*regular_index*/ 0, /*pinned_index*/ 2);
  [[EarlGrey selectElementWithMatcher:PinnedView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
