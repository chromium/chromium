// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using base::test::ios::kWaitForUIElementTimeout;
using ::base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Identifer for cell at given `index` in the tab grid.
NSString* IdentifierForRegularCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u",
                                    TabStripCollectionViewConstants
                                        .tabStripTabCellPrefixIdentifier,
                                    index];
}

// Matcher for the reguar cell at the given `index`.
id<GREYMatcher> RegularCellAtIndex(unsigned int index) {
  return grey_allOf(
      grey_accessibilityID(IdentifierForRegularCellAtIndex(index)),
      grey_kindOfClassName(@"TabStripTabCell"), grey_sufficientlyVisible(),
      nil);
}

// Finds the element with the given `identifier` of given `type`.
XCUIElement* GetElementMatchingIdentifier(XCUIApplication* app,
                                          NSString* identifier,
                                          XCUIElementType type) {
  XCUIElementQuery* query = [[app.windows.firstMatch
      descendantsMatchingType:type] matchingIdentifier:identifier];
  return [query elementBoundByIndex:0];
}

// Drags and drops the cell with the given `src_cell_identifier` to the
// `dst_cell_identifier` position.
void DragDropTabStripTabCellInTabStripView(NSString* src_cell_identifier,
                                           NSString* dst_cell_identifier) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* src_element = GetElementMatchingIdentifier(
      app, src_cell_identifier, XCUIElementTypeCell);
  XCUICoordinate* start_point =
      [src_element coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];

  XCUIElement* dst_element = GetElementMatchingIdentifier(
      app, dst_cell_identifier, XCUIElementTypeCell);
  XCUICoordinate* end_point =
      [dst_element coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];

  [start_point pressForDuration:1.5
           thenDragToCoordinate:end_point
                   withVelocity:XCUIGestureVelocityDefault
            thenHoldForDuration:1.0];
}

// Checks that the regular cell matching `tab_title` moved to `tab_index`
void AssertRegularCellMovedToNewPosition(unsigned int tab_index,
                                         NSString* tab_title) {
  ConditionBlock condition = ^{
    NSError* error = nil;

    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     RegularCellAtIndex(tab_index),
                                     grey_descendant(grey_text(tab_title)),
                                     nil)] assertWithMatcher:grey_notNil()
                                                       error:&error];
    return !error;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"The drag drop action has failed.");
}

}  // namespace

// Tests for the tab strip drag drop interactions on iPad.
@interface TabStripDragDropTestCase : ChromeTestCase
@end

@implementation TabStripDragDropTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kModernTabStrip);
  return config;
}

// Checks that dragging a regular cell to a new position correctly moves the
// cell.
// TODO(crbug.com/40285917): Test is flaky on simluator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testDragTabStripTabCellInTabStripView \
  FLAKY_testDragTabStripTabCellInTabStripView
#else
#define MAYBE_testDragTabStripTabCellInTabStripView \
  testDragTabStripTabCellInTabStripView
#endif
- (void)MAYBE_testDragTabStripTabCellInTabStripView {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Tab0: New Tab
  // Tab1: Chrome URLs
  // Tab2: About Version

  // Move Tab0 to Tab2.
  DragDropTabStripTabCellInTabStripView(IdentifierForRegularCellAtIndex(0),
                                        IdentifierForRegularCellAtIndex(2));
  AssertRegularCellMovedToNewPosition(/*tab_index*/ 2,
                                      /*tab_title*/ @"New Tab");

  // Tab0: Chrome URLs
  // Tab1: About Version
  // Tab2: New Tab

  // Move Tab1 to Tab0.
  DragDropTabStripTabCellInTabStripView(IdentifierForRegularCellAtIndex(1),
                                        IdentifierForRegularCellAtIndex(0));
  AssertRegularCellMovedToNewPosition(/*tab_index*/ 0,
                                      /*tab_title*/ @"About Version");
}

@end
