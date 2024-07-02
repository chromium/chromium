// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_CARD_SIDE_SWIPE_VIEW_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_CARD_SIDE_SWIPE_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"

using TabSwipeHandler = void (^)(int destinationWebStateIndex);

@class SideSwipeGestureRecognizer;
@protocol SideSwipeToolbarSnapshotProviding;
class WebStateList;

@interface CardSideSwipeView : UIView

@property(nonatomic, weak) id<SideSwipeMediatorDelegate> delegate;
// Snapshot provider for top and bottom toolbars.
@property(nonatomic, weak) id<SideSwipeToolbarSnapshotProviding>
    toolbarSnapshotProvider;
// Space reserved at the top for the toolbar.
@property(nonatomic, assign) CGFloat topMargin;

- (instancetype)initWithFrame:(CGRect)frame
                    topMargin:(CGFloat)margin
                 webStateList:(WebStateList*)webStateList;
- (void)updateViewsForDirection:(UISwipeGestureRecognizerDirection)direction;
- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture
      actionBeforeTabSwitch:(TabSwipeHandler)completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_CARD_SIDE_SWIPE_VIEW_H_
