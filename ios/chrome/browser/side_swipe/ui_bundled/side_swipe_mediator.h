// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mutator.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_navigation_delegate.h"

class FullscreenController;
@class LayoutGuideCenter;
@protocol HelpCommands;
@protocol SideSwipeNavigationDelegate;
@protocol SideSwipeToolbarInteracting;
@protocol SideSwipeToolbarSnapshotProviding;
@protocol TabStripHighlighting;
@protocol LensOverlayCommands;
@protocol SideSwipeConsumer;

class WebStateList;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

@protocol SideSwipeMediatorDelegate
@required
// Called when the horizontal stack view is done and should be removed.
- (void)sideSwipeViewDismissAnimationDidEnd:(UIView*)sideSwipeView;

// View that will be animated alongside the swipe by setting its frame.
//
// If the animation is contained within the browser's content area,
// use sideSwipeContentView for the swipe animation.
//
// If the animation includes browser chrome elements (e.g., toolbars),
// use sideSwipeFullscreenView for the swipe animation.
//
// Example: Swiping between the Lens overlay UI and a normal web page
// requires sideSwipeFullscreenView because the toolbars are involved.
- (UIView*)sideSwipeContentView;
- (UIView*)sideSwipeFullscreenView;

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
@interface SideSwipeMediator : NSObject <SideSwipeMutator,
                                         SideSwipeNavigationDelegate,
                                         UIGestureRecognizerDelegate>

@property(nonatomic, assign) BOOL inSwipe;
@property(nonatomic, weak) id<SideSwipeMediatorDelegate> swipeDelegate;
@property(nonatomic, weak) id<SideSwipeToolbarInteracting>
    toolbarInteractionHandler;
// Snapshot provider for top and bottom toolbars.
@property(nonatomic, weak) id<SideSwipeToolbarSnapshotProviding>
    toolbarSnapshotProvider;

@property(nonatomic, weak) id<TabStripHighlighting> tabStripDelegate;

@property(nonatomic, assign) FullscreenController* fullscreenController;

@property(nonatomic) feature_engagement::Tracker* engagementTracker;

@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Handler for in-product help tips.
@property(nonatomic, weak) id<HelpCommands> helpHandler;

// The side swipe consumer. It will mainly receive webstate updates.
@property(nonatomic, weak) id<SideSwipeConsumer> consumer;

// Initializer.
- (instancetype)initWithFullscreenController:
                    (FullscreenController*)fullscreenController
                                webStateList:(WebStateList*)webStateList;

// Disconnects the mediator.
- (void)disconnect;

// Set up swipe gesture recognizers.
- (void)addHorizontalGesturesToView:(UIView*)view;

// Enable or disable the side swipe gesture recognizer.
- (void)setEnabled:(BOOL)enabled;

// Whether or not a side swipe is in progress.
- (BOOL)isSideSwipeInProgress;

// Resets the swipeDelegate's contentView frame origin x position to zero if
// there is an active swipe.
- (void)resetContentView;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_H_
