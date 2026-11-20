// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_VERTICAL_SIDEBAR_VIEW_TESTING_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_VERTICAL_SIDEBAR_VIEW_TESTING_H_

#import "ios/chrome/browser/shared/ui/elements/vertical_sidebar_view.h"

// Testing category exposing a private method of `VerticalSidebarView`, so that
// tests can inject the system's bar region, which only exists in a window of
// an iPhone Duo cover display (and only with the iOS 27.1+ SDK).
@interface VerticalSidebarView (Testing)

// Centers the pills on `guide`, the system's layout region for a bar on
// `outerEdge`, and keeps them within its vertical span. Replaces the previous
// region's constraints.
- (void)constrainPillsToBarRegionGuide:(UILayoutGuide*)guide;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_VERTICAL_SIDEBAR_VIEW_TESTING_H_
