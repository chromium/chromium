// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_SIDE_SWIPE_TOOLBAR_INTERACTING_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_SIDE_SWIPE_TOOLBAR_INTERACTING_H_

#import <UIKit/UIKit.h>

// Protocol used by SideSwipe to interact with the toolbar.
@protocol SideSwipeToolbarInteracting

// Returns whether the `point` is inside a toolbar's frame. The `point` must be
// in the window coordinates.
- (BOOL)isInsideToolbar:(CGPoint)point;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_SIDE_SWIPE_TOOLBAR_INTERACTING_H_
