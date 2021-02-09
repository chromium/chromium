// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"

#import "base/mac/foundation_util.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

BOOL LongPressAndDragToEdge(NSString* accessibilityIdentifier,
                            GREYContentEdge edge) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElementQuery* query =
      [[app.windows descendantsMatchingType:XCUIElementTypeAny]
          matchingIdentifier:accessibilityIdentifier];

  if (query.count == 0)
    return NO;
  XCUIElement* dragElement = [query elementBoundByIndex:0];

  CGVector edgeCenter;
  switch (edge) {
    case kGREYContentEdgeLeft:
      edgeCenter = CGVectorMake(0.0, 0.5);
      break;
    case kGREYContentEdgeRight:
      edgeCenter = CGVectorMake(1.0, 0.5);
      break;
    case kGREYContentEdgeTop:
      edgeCenter = CGVectorMake(0.5, 0.0);
      break;
    case kGREYContentEdgeBottom:
      edgeCenter = CGVectorMake(0.5, 1.0);
      break;
  }

  XCUICoordinate* startPoint =
      [dragElement coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];
  XCUICoordinate* endPoint = [app coordinateWithNormalizedOffset:edgeCenter];

  [startPoint pressForDuration:1.5
          thenDragToCoordinate:endPoint
                  withVelocity:XCUIGestureVelocityDefault
           thenHoldForDuration:1.0];

  return YES;
}

}  // namespace chrome_test_util
