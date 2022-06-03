// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIDE_SWIPE_CARD_SIDE_SWIPE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_SIDE_SWIPE_CARD_SIDE_SWIPE_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"

@class SideSwipeGestureRecognizer;
@protocol SideSwipeToolbarSnapshotProviding;
class WebStateList;

@interface CardSideSwipeView : UIView

@property(nonatomic, weak) id<SideSwipeControllerDelegate> delegate;
// Snapshot provider for the top toolbar.
@property(nonatomic, weak) id<SideSwipeToolbarSnapshotProviding>
    topToolbarSnapshotProvider;
// Snapshot provider for the bottom toolbar.
@property(nonatomic, weak) id<SideSwipeToolbarSnapshotProviding>
    bottomToolbarSnapshotProvider;
// Space reserved at the top for the toolbar.
@property(nonatomic, assign) CGFloat topMargin;

- (instancetype)initWithFrame:(CGRect)frame
                    topMargin:(CGFloat)margin
                 webStateList:(WebStateList*)webStateList;
- (void)updateViewsForDirection:(UISwipeGestureRecognizerDirection)direction;
- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture;

@end

#endif  // IOS_CHROME_BROWSER_UI_SIDE_SWIPE_CARD_SIDE_SWIPE_VIEW_H_
