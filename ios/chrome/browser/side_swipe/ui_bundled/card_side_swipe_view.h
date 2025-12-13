// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_CARD_SIDE_SWIPE_VIEW_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_CARD_SIDE_SWIPE_VIEW_H_

#import <UIKit/UIKit.h>

using TabSwipeHandler = void (^)(int destinationWebStateIndex);

@protocol CardSwipeViewDelegate;
@class SideSwipeGestureRecognizer;
@protocol SideSwipeToolbarSnapshotProviding;
class SnapshotBrowserAgent;
class WebStateList;

@interface CardSideSwipeView : UIView

@property(nonatomic, weak) id<CardSwipeViewDelegate> delegate;
// Snapshot provider for top and bottom toolbars.
@property(nonatomic, weak) id<SideSwipeToolbarSnapshotProviding>
    toolbarSnapshotProvider;
// Space reserved at the top for the toolbar.
@property(nonatomic, assign) CGFloat topMargin;

// Inits with the view `frame`, top `margin` and `webStateList`.
- (instancetype)initWithFrame:(CGRect)frame
                    topMargin:(CGFloat)margin
                 webStateList:(WebStateList*)webStateList
         snapshotBrowserAgent:(SnapshotBrowserAgent*)snapshotBrowserAgent;

// Sets up left and right card views depending on current WebState and swipe
// direction.
- (void)updateViewsForDirection:(UISwipeGestureRecognizerDirection)direction;

// Update layout with new touch event.
- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture
      actionBeforeTabSwitch:(TabSwipeHandler)completionHandler;

// Disconnects this view.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_CARD_SIDE_SWIPE_VIEW_H_
