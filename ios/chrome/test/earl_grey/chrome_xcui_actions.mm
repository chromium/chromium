// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// The splitter is 10 points wide.
const CGFloat kSplitterWidth = 10.0;

// Returns a normalized vector for the given edge.
CGVector GetNormalizedEdgeVector(GREYContentEdge edge) {
  switch (edge) {
    case kGREYContentEdgeLeft:
      return CGVectorMake(0.02, 0.5);
    case kGREYContentEdgeRight:
      return CGVectorMake(.98, 0.5);
    case kGREYContentEdgeTop:
      return CGVectorMake(0.5, 0.02);
    case kGREYContentEdgeBottom:
      return CGVectorMake(0.5, 0.98);
    default:
      return CGVectorMake(0.5, 0.5);
  }
}

NSString* GetWindowAccessibilityIdentifier(int window_number) {
  return [NSString stringWithFormat:@"%d", window_number];
}

// Finds the element with the given `identifier` of given `type` in given
// `window_number`.  If `identitifer` is nil, this will return a element for the
// window with `window_number` itself.
XCUIElement* GetElementMatchingIdentifierInWindow(XCUIApplication* app,
                                                  NSString* identifier,
                                                  int window_number,
                                                  XCUIElementType type) {
  NSString* window_id = GetWindowAccessibilityIdentifier(window_number);
  XCUIElementQuery* query = nil;
  if (identifier) {
    // Check for matching descendants.
    query = [[[app.windows matchingIdentifier:window_id]
        descendantsMatchingType:type] matchingIdentifier:identifier];
  } else {
    // Check for window itself.
    query = [app.windows matchingIdentifier:window_id];
  }

  if (query.count == 0)
    return nil;

  return [query elementBoundByIndex:0];
}

// Long press at `start_point` and drag to `end_point`, with fixed press and
// hold druations and drag velocity.
void LongPressAndDragBetweenCoordinates(XCUICoordinate* start_point,
                                        XCUICoordinate* end_point) {
  [start_point pressForDuration:1.5
           thenDragToCoordinate:end_point
                   withVelocity:XCUIGestureVelocityDefault
            thenHoldForDuration:1.0];
}

// Long press on `src_element`'s center then drag to the point in `dst_element`
// defined by `dst_normalized_offset`. Returns NO if either element is nil, YES
// otherwise.
BOOL LongPressAndDragBetweenElements(XCUIElement* src_element,
                                     XCUIElement* dst_element,
                                     CGVector dst_normalized_offset) {
  if (!src_element || !dst_element)
    return NO;

  XCUICoordinate* start_point =
      [src_element coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];
  XCUICoordinate* end_point =
      [dst_element coordinateWithNormalizedOffset:dst_normalized_offset];

  LongPressAndDragBetweenCoordinates(start_point, end_point);
  return YES;
}

}  // namespace

namespace chrome_test_util {

BOOL LongPressCellAndDragToEdge(NSString* accessibility_identifier,
                                GREYContentEdge edge,
                                int window_number) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* drag_element = GetElementMatchingIdentifierInWindow(
      app, accessibility_identifier, window_number, XCUIElementTypeCell);

  // `app` is still an element, so it can just be passed in directly here.
  return LongPressAndDragBetweenElements(drag_element, app,
                                         GetNormalizedEdgeVector(edge));
}

BOOL LongPressCellAndDragToOffsetOf(NSString* src_accessibility_identifier,
                                    int src_window_number,
                                    NSString* dst_accessibility_identifier,
                                    int dst_window_number,
                                    CGVector dst_normalized_offset) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* src_element = GetElementMatchingIdentifierInWindow(
      app, src_accessibility_identifier, src_window_number,
      XCUIElementTypeCell);

  XCUIElement* dst_element = GetElementMatchingIdentifierInWindow(
      app, dst_accessibility_identifier, dst_window_number,
      XCUIElementTypeCell);

  return LongPressAndDragBetweenElements(src_element, dst_element,
                                         dst_normalized_offset);
}

BOOL LongPressLinkAndDragToView(NSString* src_accessibility_identifier,
                                int src_window_number,
                                NSString* dst_accessibility_identifier,
                                int dst_window_number,
                                CGVector dst_normalized_offset) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* src_element = GetElementMatchingIdentifierInWindow(
      app, src_accessibility_identifier, src_window_number,
      XCUIElementTypeLink);

  XCUIElement* dst_element = GetElementMatchingIdentifierInWindow(
      app, dst_accessibility_identifier, dst_window_number, XCUIElementTypeAny);

  return LongPressAndDragBetweenElements(src_element, dst_element,
                                         dst_normalized_offset);
}

BOOL LongPressLinkAndDragToView(NSString* src_accessibility_identifier,
                                NSString* dst_accessibility_identifier) {
  return LongPressLinkAndDragToView(src_accessibility_identifier, 0,
                                    dst_accessibility_identifier, 0,
                                    CGVectorMake(0.5, 0.5));
}

BOOL DragWindowSplitterToSize(int first_window_number,
                              int second_window_number,
                              CGFloat first_window_normalized_screen_size) {
  CGRect first_rect =
      [ChromeEarlGrey screenPositionOfScreenWithNumber:first_window_number];
  if (CGRectIsEmpty(first_rect))
    return NO;

  CGRect second_rect =
      [ChromeEarlGrey screenPositionOfScreenWithNumber:second_window_number];
  if (CGRectIsEmpty(second_rect))
    return NO;

  // Fail early if a floating window was passed in in one of the windows.
  if (first_rect.origin.x != second_rect.origin.x &&
      first_rect.origin.y != second_rect.origin.y)
    return NO;

  // Use device orientation to see if it is one of the modes where we need to
  // invert ltr, because the two rects returned above are always related to
  // UIDeviceOrientationPortrait, and always defined with x,y being the top
  // left corner and width and height always positive.
  UIDeviceOrientation orientation =
      [[GREY_REMOTE_CLASS_IN_APP(UIDevice) currentDevice] orientation];
  BOOL inverted_display = orientation == UIDeviceOrientationLandscapeRight ||
                          orientation == UIDeviceOrientationPortraitUpsideDown;

  // Select leftmost window as reference.
  BOOL ltr = first_rect.origin.x < second_rect.origin.x;
  BOOL landscape = NO;
  if (first_rect.origin.x == second_rect.origin.x) {
    ltr = first_rect.origin.y < second_rect.origin.y;
    landscape = YES;
  }
  if (inverted_display)
    ltr = !ltr;
  int left_window = ltr ? first_window_number : second_window_number;

  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElementQuery* query = [app.windows
      matchingIdentifier:GetWindowAccessibilityIdentifier(left_window)];

  if (query.count == 0)
    return NO;
  XCUIElement* element = [query elementBoundByIndex:0];

  // Start on center of splitter, right of leftmost window.
  XCUICoordinate* start_point =
      [[element coordinateWithNormalizedOffset:CGVectorMake(1.0, 0.5)]
          coordinateWithOffset:CGVectorMake(kSplitterWidth / 2.0, 0.0)];

  // Transform screen relative normalized size into offset relative to
  // 'left_window'.
  CGFloat offset;
  if (landscape) {
    CGFloat height =
        first_rect.size.height + kSplitterWidth + second_rect.size.height;
    if (ltr) {
      offset =
          first_window_normalized_screen_size * height / first_rect.size.height;
    } else {
      offset = (1.0 - first_window_normalized_screen_size) * height /
               second_rect.size.height;
    }
  } else {
    CGFloat width =
        first_rect.size.width + kSplitterWidth + second_rect.size.width;
    if (ltr) {
      offset =
          first_window_normalized_screen_size * width / first_rect.size.width;
    } else {
      offset = (1.0 - first_window_normalized_screen_size) * width /
               second_rect.size.width;
    }
  }

  XCUICoordinate* end_point =
      [element coordinateWithNormalizedOffset:CGVectorMake(offset, 0.5)];

  [start_point pressForDuration:0.1
           thenDragToCoordinate:end_point
                   withVelocity:XCUIGestureVelocityFast
            thenHoldForDuration:0.2];

  return YES;
}

BOOL TapAtOffsetOf(NSString* accessibility_identifier,
                   int window_number,
                   CGVector normalized_offset) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* element = GetElementMatchingIdentifierInWindow(
      app, accessibility_identifier, window_number, XCUIElementTypeAny);

  if (!element)
    return NO;

  XCUICoordinate* tap_point =
      [element coordinateWithNormalizedOffset:normalized_offset];
  [tap_point tap];

  return YES;
}

BOOL TypeText(NSString* accessibility_identifier,
              int window_number,
              NSString* text) {
  XCUIApplication* app = [[XCUIApplication alloc] init];

  XCUIElement* element = GetElementMatchingIdentifierInWindow(
      app, accessibility_identifier, window_number, XCUIElementTypeTextField);

  if (!element)
    return NO;

  [element typeText:text];

  return YES;
}

}  // namespace chrome_test_util
