// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The splitter is 10 points wide.
const CGFloat kSplitterWidth = 10.0;

// Returns a normalized vector for the given edge.
CGVector GetNormalizedEdgeVector(GREYContentEdge edge) {
  switch (edge) {
    case kGREYContentEdgeLeft:
      return CGVectorMake(0.02, 0.5);
      break;
    case kGREYContentEdgeRight:
      return CGVectorMake(.98, 0.5);
      break;
    case kGREYContentEdgeTop:
      return CGVectorMake(0.5, 0.02);
      break;
    case kGREYContentEdgeBottom:
      return CGVectorMake(0.5, 0.98);
      break;
    default:
      return CGVectorMake(0.5, 0.5);
  }
}

NSString* GetWindowAccessibilityIdentifier(int window_number) {
  return [NSString stringWithFormat:@"%d", window_number];
}

// Creates a query for the given |identifier| of given |type| in given
// |window_number|.  If |identitifer| is nil, this will return a query for the
// window with |window_number| itself.
XCUIElementQuery* GetQueryMatchingIdentifierInWindow(XCUIApplication* app,
                                                     NSString* identifier,
                                                     int window_number,
                                                     XCUIElementType type) {
  NSString* window_id = GetWindowAccessibilityIdentifier(window_number);
  if (identifier) {
    // Check for matching descendants.
    return [[[app.windows matchingIdentifier:window_id]
        descendantsMatchingType:type] matchingIdentifier:identifier];
  }
  // Check for window itself.
  return [app.windows matchingIdentifier:window_id];
}

}  // namespace

namespace chrome_test_util {

BOOL LongPressCellAndDragToEdge(NSString* accessibility_identifier,
                                GREYContentEdge edge,
                                int window_number) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElementQuery* query = GetQueryMatchingIdentifierInWindow(
      app, accessibility_identifier, window_number, XCUIElementTypeCell);

  if (query.count == 0)
    return NO;
  XCUIElement* drag_element = [query elementBoundByIndex:0];

  XCUICoordinate* start_point =
      [drag_element coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];
  XCUICoordinate* end_point =
      [app coordinateWithNormalizedOffset:GetNormalizedEdgeVector(edge)];

  [start_point pressForDuration:1.5
           thenDragToCoordinate:end_point
                   withVelocity:XCUIGestureVelocityDefault
            thenHoldForDuration:1.0];

  return YES;
}

BOOL LongPressCellAndDragToOffsetOf(NSString* src_accessibility_identifier,
                                    int src_window_number,
                                    NSString* dst_accessibility_identifier,
                                    int dst_window_number,
                                    CGVector dst_normalized_offset) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElementQuery* src_query = GetQueryMatchingIdentifierInWindow(
      app, src_accessibility_identifier, src_window_number,
      XCUIElementTypeCell);

  XCUIElementQuery* dst_query = GetQueryMatchingIdentifierInWindow(
      app, dst_accessibility_identifier, dst_window_number,
      XCUIElementTypeCell);

  if (src_query.count == 0 || dst_query.count == 0)
    return NO;
  XCUIElement* src_element = [src_query elementBoundByIndex:0];
  XCUIElement* dst_element = [dst_query elementBoundByIndex:0];

  XCUICoordinate* start_point =
      [src_element coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];
  XCUICoordinate* end_point =
      [dst_element coordinateWithNormalizedOffset:dst_normalized_offset];

  [start_point pressForDuration:1.5
           thenDragToCoordinate:end_point
                   withVelocity:XCUIGestureVelocityDefault
            thenHoldForDuration:1.0];

  return YES;
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
  XCUIElementQuery* query = GetQueryMatchingIdentifierInWindow(
      app, accessibility_identifier, window_number, XCUIElementTypeAny);

  if (query.count == 0)
    return NO;

  XCUIElement* element = [query elementBoundByIndex:0];
  XCUICoordinate* tap_point =
      [element coordinateWithNormalizedOffset:normalized_offset];
  [tap_point tap];

  return YES;
}

BOOL TypeText(NSString* accessibility_identifier,
              int window_number,
              NSString* text) {
  XCUIApplication* app = [[XCUIApplication alloc] init];

  XCUIElement* element = nil;
  if (@available(iOS 13, *)) {
    XCUIElementQuery* query = GetQueryMatchingIdentifierInWindow(
        app, accessibility_identifier, window_number, XCUIElementTypeTextField);
    if (query.count > 0)
      element = [query elementBoundByIndex:0];
  } else {
    // [XCUIElementQuery matchingIdentifier:] doesn't work in iOS 12.
    XCUIElementQuery* query = app.textFields;
    for (unsigned int i = 0; i < query.count; i++, element = nil) {
      element = [query elementBoundByIndex:i];
      if ([element.identifier isEqualToString:accessibility_identifier])
        break;
    }
  }

  if (!element)
    return NO;

  [element typeText:text];

  return YES;
}

}  // namespace chrome_test_util
