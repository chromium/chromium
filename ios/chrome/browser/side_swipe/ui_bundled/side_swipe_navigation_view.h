// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_NAVIGATION_VIEW_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_NAVIGATION_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/side_swipe/ui_bundled/horizontal_pan_gesture_handler.h"

@class SideSwipeGestureRecognizer;

// Accessory view showing forward or back arrow while the main target view
// is dragged from side to side.
@interface SideSwipeNavigationView : UIView <HorizontalPanGestureHandler>

// Initialize with direction.
- (instancetype)initWithFrame:(CGRect)frame
                withDirection:(UISwipeGestureRecognizerDirection)direction
                  canNavigate:(BOOL)canNavigate
                        image:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_NAVIGATION_VIEW_H_
