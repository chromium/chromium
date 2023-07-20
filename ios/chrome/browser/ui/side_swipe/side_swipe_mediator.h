// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/snapshots/snapshot_generator_delegate.h"

class FullscreenController;
@protocol SideSwipeToolbarInteracting;
@protocol SideSwipeToolbarSnapshotProviding;
class SnapshotBrowserAgent;
@protocol TabStripHighlighting;
class WebStateList;

// Notification sent when the user starts a side swipe (on tablet).
extern NSString* const kSideSwipeWillStartNotification;
// Notification sent when the user finishes a side swipe (on tablet).
extern NSString* const kSideSwipeDidStopNotification;

@protocol SideSwipeMediatorDelegate
@required
// Called when the horizontal stack view is done and should be removed.
- (void)sideSwipeViewDismissAnimationDidEnd:(UIView*)sideSwipeView;
// Returns the main content view.
- (UIView*)sideSwipeContentView;
// Makes `tab` the currently visible tab, displaying its view.
- (void)sideSwipeRedisplayTabView;
// Controls the visibility of views such as the findbar, infobar and voice
// search bar.
- (void)updateAccessoryViewsForSideSwipeWithVisibility:(BOOL)visible;
// Returns the height of the header view for the current tab.
- (CGFloat)headerHeightForSideSwipe;
// Returns `YES` if side swipe should be blocked from initiating, such as when
// voice search is up, or if the tools menu is enabled.
- (BOOL)preventSideSwipe;
// Returns whether a swipe on the toolbar can start.
- (BOOL)canBeginToolbarSwipe;
// Returns the top toolbar's view.
- (UIView*)topToolbarView;

@end

// Controls how an edge gesture is processed, either as tab change or a page
// change.  For tab changes two full screen CardSideSwipeView views are dragged
// across the screen. For page changes the SideSwipeMediatorDelegate
// `contentView` is moved across the screen and a SideSwipeNavigationView is
// shown in the remaining space.
@interface SideSwipeMediator : NSObject <UIGestureRecognizerDelegate>

@property(nonatomic, assign) BOOL inSwipe;
@property(nonatomic, weak) id<SideSwipeMediatorDelegate> swipeDelegate;
@property(nonatomic, weak) id<SideSwipeToolbarInteracting>
    toolbarInteractionHandler;
// Snapshot provider for top and bottom toolbars.
@property(nonatomic, weak) id<SideSwipeToolbarSnapshotProviding>
    toolbarSnapshotProvider;

@property(nonatomic, weak) id<SnapshotGeneratorDelegate> snapshotDelegate;
@property(nonatomic, weak) id<TabStripHighlighting> tabStripDelegate;

@property(nonatomic, assign) FullscreenController* fullscreenController;

// Initializer.
- (instancetype)
    initWithFullscreenController:(FullscreenController*)fullscreenController
            snapshotBrowserAgent:(SnapshotBrowserAgent*)snapshotBrowserAgent
                    webStateList:(WebStateList*)webStateList;

// Disconnects the mediator.
- (void)disconnect;

// Set up swipe gesture recognizers.
- (void)addHorizontalGesturesToView:(UIView*)view;

// Enable or disable the side swipe gesture recognizer.
- (void)setEnabled:(BOOL)enabled;

// Returns `NO` if the device should not rotate.
- (BOOL)shouldAutorotate;

// Resets the swipeDelegate's contentView frame origin x position to zero if
// there is an active swipe.
- (void)resetContentView;

@end

#endif  // IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_MEDIATOR_H_
