// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class ActorOverlayViewController;
@class LayoutGuideCenter;
@class SceneLayoutState;

// Delegate for user interactions with `ActorOverlayViewController`.
@protocol ActorOverlayViewControllerDelegate <NSObject>

// Called when the user taps on the overlay scrim view.
- (void)actorOverlayViewControllerDidTapScrim:
    (ActorOverlayViewController*)viewController;

@end

// A view controller managing the actor overlay view.
@interface ActorOverlayViewController : UIViewController

// Delegate notified of user interactions with the overlay.
@property(nonatomic, weak) id<ActorOverlayViewControllerDelegate> delegate;

// The layout state observed by this view controller.
@property(nonatomic, weak) SceneLayoutState* layoutState;

// Whether the touch-blocking scrim view is visible. Defaults to `YES`. When
// `NO`, only the border glow is shown and touches pass through to underlying
// views.
@property(nonatomic, assign, getter=isScrimVisible) BOOL scrimVisible;

// Initializes the view controller using `browserCenter` to anchor subviews to
// browser layout guides, `scrimColor` as the base scrim color, and `glowColor`
// as the base glow color.
- (instancetype)initWithBrowserLayoutGuideCenter:
                    (LayoutGuideCenter*)browserCenter
                                      scrimColor:(UIColor*)scrimColor
                                       glowColor:(UIColor*)glowColor
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_
