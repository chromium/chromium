// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/device_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
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

using ::base::test::ios::kWaitForUIElementTimeout;
using ::base::test::ios::WaitUntilConditionOrTimeout;

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

// Finds the element with the given `identifier` of given `type`.
XCUIElement* GetElementMatchingLabel(XCUIApplication* app,
                                     NSString* label,
                                     XCUIElementType type) {
  NSPredicate* predicate =
      [NSPredicate predicateWithBlock:^BOOL(id<XCUIElementAttributes> item,
                                            NSDictionary* bindings) {
        return [item.label isEqualToString:label];
      }];

  XCUIElementQuery* query = [[app.windows.firstMatch
      descendantsMatchingType:type] matchingPredicate:predicate];
  return [query elementBoundByIndex:0];
}

// Drags and drops the cell with the given `cell_identifier` in the pinned view.
void DragDropCellInPinnedView(NSString* cell_identifier) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* src_element =
      GetElementMatchingIdentifier(app, cell_identifier, XCUIElementTypeCell);
  XCUICoordinate* start_point =
      [src_element coordinateWithNormalizedOffset:CGVectorMake(0.1, 0.1)];

  XCUIElement* dst_element = GetElementMatchingLabel(
      app, l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB),
      XCUIElementTypeAny);

  // Supposed position of the pinned view.
  // The pinned view is hidden when there is no pinned tabs. We can't determine
  // its position precisely.
  XCUICoordinate* end_point =
      [dst_element coordinateWithNormalizedOffset:CGVectorMake(-0.8, -0.5)];

  [start_point pressForDuration:1.5
           thenDragToCoordinate:end_point
                   withVelocity:XCUIGestureVelocityDefault
            thenHoldForDuration:1.0];
}

// Drags and drops the cell with the given `cell_identifier` in the regular
// grid.
void DragDropCellInRegularGrid(NSString* cell_identifier) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* src_element =
      GetElementMatchingIdentifier(app, cell_identifier, XCUIElementTypeCell);
  XCUICoordinate* start_point =
      [src_element coordinateWithNormalizedOffset:CGVectorMake(0.1, 0.1)];

  // Supposed position of the regular grid.
  XCUICoordinate* end_point =
      [app coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.4)];

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

// Checks that the pinned cell at `pinned_index` index has been moved to the
// regular grid at `regular_index` index .
void AssertPinnedCellMovedToRegularGrid(unsigned int pinned_index,
                                        unsigned int regular_index) {
  ConditionBlock condition = ^{
    NSError* error1 = nil;
    NSError* error2 = nil;

    [[EarlGrey selectElementWithMatcher:PinnedCellAtIndex(pinned_index)]
        assertWithMatcher:grey_nil()
                    error:&error1];
    [[EarlGrey selectElementWithMatcher:RegularCellAtIndex(regular_index)]
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

// Checks that the Pinned Tabs feature is disabled on iPad.
- (void)testDragRegularTabOniPad {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPhone.");
  }

  [ChromeEarlGreyUI openTabGrid];

  // The pinned view should not be visible.
  [[EarlGrey selectElementWithMatcher:PinnedView()]
      assertWithMatcher:grey_notVisible()];

  [[EarlGrey selectElementWithMatcher:RegularCellAtIndex(0)]
      assertWithMatcher:grey_notNil()];

  // Try to drag the first cell in the pinned view.
  DragDropCellInPinnedView(IdentifierForRegularCellAtIndex(0));

  // Check that the cell has not been moved in the pinned view.
  ConditionBlock condition = ^{
    NSError* error1 = nil;
    NSError* error2 = nil;

    [[EarlGrey selectElementWithMatcher:RegularCellAtIndex(0)]
        assertWithMatcher:grey_notNil()
                    error:&error1];
    [[EarlGrey selectElementWithMatcher:PinnedCellAtIndex(0)]
        assertWithMatcher:grey_nil()
                    error:&error2];

    return !error1 && !error2;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"The Pinned Tabs feature is not disabled on iPad.");
  [[EarlGrey selectElementWithMatcher:PinnedView()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that dragging a regular tab and dropping it in the pinned view moves
// it in the pinned view.
// TODO(crbug.com/40285917): Test is flaky on simluator. Re-enable the test.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testDragRegularTabInPinnedView \
  FLAKY_testDragRegularTabInPinnedView
#else
#define MAYBE_testDragRegularTabInPinnedView testDragRegularTabInPinnedView
#endif
- (void)MAYBE_testDragRegularTabInPinnedView {
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

// Checks that dragging a pinned tab and dropping it in the regular grid moves
// it in the regular grid.
- (void)testDragPinnedTabInRegularGrid {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }
  // TODO(crbug.com/40923015): Failing on iOS17, and iOS15.5 for
  // ios-simulator-noncq.
  XCTSkip(@"Failing on iOS17 and iOS15.5");

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];

  // Drag both tabs in the pinned view.
  DragDropCellInPinnedView(IdentifierForRegularCellAtIndex(1));
  AssertRegularCellMovedToPinnedView(/*regular_index*/ 1, /*pinned_index*/ 0);
  DragDropCellInPinnedView(IdentifierForRegularCellAtIndex(0));
  AssertRegularCellMovedToPinnedView(/*regular_index*/ 0, /*pinned_index*/ 1);

  [[EarlGrey selectElementWithMatcher:PinnedView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Drag the second pinned cell in the regular grid.
  DragDropCellInRegularGrid(IdentifierForPinnedCellAtIndex(1));
  AssertPinnedCellMovedToRegularGrid(/*pinned_index*/ 1, /*regular_index*/ 0);
  [[EarlGrey selectElementWithMatcher:PinnedView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Drag the first (and last) pinned cell in the regular grid.
  DragDropCellInRegularGrid(IdentifierForPinnedCellAtIndex(0));
  AssertPinnedCellMovedToRegularGrid(/*pinned_index*/ 0, /*regular_index*/ 1);

  // Check that the pinned view is hidden when its last item has been removed.
  ConditionBlock condition = ^{
    NSError* error = nil;

    [[EarlGrey selectElementWithMatcher:PinnedView()]
        assertWithMatcher:grey_notVisible()
                    error:&error];

    return !error;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"The pinned view is still visible.");
}

@end
