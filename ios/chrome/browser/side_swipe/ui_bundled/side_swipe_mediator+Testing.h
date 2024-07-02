// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_TESTING_H_

namespace web {
class WebState;
}  // namespace web

// Testing category to expose private methods of SideSwipeMediator
// for tests.
@interface SideSwipeMediator (Testing)

// Whether to allow navigating from the leading edge.
@property(nonatomic, assign) BOOL leadingEdgeNavigationEnabled;

// Whether to allow navigating from the trailing edge.
@property(nonatomic, assign) BOOL trailingEdgeNavigationEnabled;

- (void)updateNavigationEdgeSwipeForWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_TESTING_H_
