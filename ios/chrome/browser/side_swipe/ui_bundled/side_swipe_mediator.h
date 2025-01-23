// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_H_

#import <UIKit/UIKit.h>

class FullscreenController;
@class LayoutGuideCenter;
@protocol HelpCommands;
@protocol SideSwipeToolbarInteracting;
@protocol SideSwipeToolbarSnapshotProviding;
@protocol TabStripHighlighting;
@protocol LensOverlayCommands;
class WebStateList;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

// Notification sent when the user starts a side swipe (on tablet).
extern NSString* const kSideSwipeWillStartNotification;
// Notification sent when the user finishes a side swipe (on tablet).
extern NSString* const kSideSwipeDidStopNotification;

enum class SwipeType { NONE, CHANGE_TAB, CHANGE_PAGE };

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
@interface SideSwipeMediator : NSObject <UIGestureRecognizerDelegate>

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

// Returns `NO` if the device should not rotate.
- (BOOL)shouldAutorotate;

// Resets the swipeDelegate's contentView frame origin x position to zero if
// there is an active swipe.
- (void)resetContentView;

// Performs an animation that simulates a swipe with `swipeType` in `direction`.
- (void)animateSwipe:(SwipeType)swipeType
         inDirection:(UISwipeGestureRecognizerDirection)direction;

// Prepares the view for a slide-in overlay navigation transition in the
// specified direction.
//
// This method sets up for an overlay navigation transition where the entire
// screen is initially positioned offscreen. A snapshot of the screen is taken
// and used to replace the current fullscreen view, creating a seamless slide-in
// effect when `slideToCenterAnimated` is called.
//
// Important: After calling this method, you must call `slideToCenterAnimated`
// to restore the fullscreen view to its original position and complete the
// transition.
- (void)prepareForSlideInDirection:(UISwipeGestureRecognizerDirection)direction;

// Restores the fullscreen view to its original position with an animation.
//
// This method animates the fullscreen view back to its original onscreen
// position after it has been moved offscreen using
// `prepareForSlideInDirection:`.
- (void)slideToCenterAnimated;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_H_
