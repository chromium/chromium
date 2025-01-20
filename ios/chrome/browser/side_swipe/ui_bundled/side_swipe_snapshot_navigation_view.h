// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_SNAPSHOT_NAVIGATION_VIEW_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_SNAPSHOT_NAVIGATION_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/side_swipe/ui_bundled/horizontal_pan_gesture_handler.h"

// A view showing a snapshot image while the target view is dragged
// from side to side.
@interface SideSwipeSnapshotNavigationView
    : UIView <HorizontalPanGestureHandler>

// Initialize the view with a given frame and a snapshot image.
- (instancetype)initWithFrame:(CGRect)frame snapshot:(UIImage*)snapshotImage;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_SNAPSHOT_NAVIGATION_VIEW_H_
