// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_NAVIGATION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_NAVIGATION_VIEW_H_

#import <UIKit/UIKit.h>

@class SideSwipeGestureRecognizer;

// Accessory view showing forward or back arrow while the main target view
// is dragged from side to side.
@interface SideSwipeNavigationView : UIView

@property(nonatomic, weak) UIView* targetView;

// Initialize with direction.
- (instancetype)initWithFrame:(CGRect)frame
                withDirection:(UISwipeGestureRecognizerDirection)direction
                  canNavigate:(BOOL)canNavigate
                        image:(UIImage*)image;

// Update views for latest gesture, and call completion blocks whether
// `threshold` is met.
- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture
     onOverThresholdCompletion:(void (^)(void))onOverThresholdCompletion
    onUnderThresholdCompletion:(void (^)(void))onUnderThresholdCompletion;
@end

#endif  // IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_NAVIGATION_VIEW_H_
