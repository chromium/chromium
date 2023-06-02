// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_CONTROLLER_PRIVATE_H_

namespace web {
class WebState;
}  // namespace web

// Class extension exposing private methods of SideSwipeController
// for testing.
@interface SideSwipeController ()

// Whether to allow navigating from the leading edge.
@property(nonatomic, assign) BOOL leadingEdgeNavigationEnabled;

// Whether to allow navigating from the trailing edge.
@property(nonatomic, assign) BOOL trailingEdgeNavigationEnabled;

- (void)updateNavigationEdgeSwipeForWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_CONTROLLER_PRIVATE_H_
