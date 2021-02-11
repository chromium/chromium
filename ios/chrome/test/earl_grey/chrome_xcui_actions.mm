// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"

#import "base/mac/foundation_util.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a normalized vector for the given edge.
CGVector GetNormalizedEdgeVector(GREYContentEdge edge) {
  switch (edge) {
    case kGREYContentEdgeLeft:
      return CGVectorMake(0.0, 0.5);
      break;
    case kGREYContentEdgeRight:
      return CGVectorMake(1.0, 0.5);
      break;
    case kGREYContentEdgeTop:
      return CGVectorMake(0.5, 0.0);
      break;
    case kGREYContentEdgeBottom:
      return CGVectorMake(0.5, 1.0);
      break;
    default:
      return CGVectorMake(0.5, 0.5);
  }
}

// Creates a query for the given |identifier| of given |type| in given
// |window_number|.  If |identitifer| is nil, this will return a query for the
// window with |window_number| itself.
XCUIElementQuery* GetQueryMatchingIdentifierInWindow(XCUIApplication* app,
                                                     NSString* identifier,
                                                     int window_number,
                                                     XCUIElementType type) {
  NSString* window_id = [NSString stringWithFormat:@"%d", window_number];
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

}  // namespace chrome_test_util
