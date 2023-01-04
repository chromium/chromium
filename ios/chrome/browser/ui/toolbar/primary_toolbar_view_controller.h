// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"
#import "ios/chrome/browser/ui/keyboard/key_command_actions.h"
#import "ios/chrome/browser/ui/orchestrator/toolbar_animatee.h"
#import "ios/chrome/browser/ui/sharing/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"

@protocol PrimaryToolbarViewControllerDelegate;
@class ViewRevealingVerticalPanHandler;

// ViewController for the primary toobar part of the adaptive toolbar. It is the
// part always displayed and containing the location bar.
@interface PrimaryToolbarViewController
    : AdaptiveToolbarViewController <ActivityServicePositioner,
                                     FullscreenUIElement,
                                     KeyCommandActions,
                                     ToolbarAnimatee,
                                     ViewRevealingAnimatee>

@property(nonatomic, weak) id<PrimaryToolbarViewControllerDelegate> delegate;

// Whether the omnibox should be hidden on NTP.
@property(nonatomic, assign) BOOL shouldHideOmniboxOnNTP;

// Pan gesture handler for the toolbar.
@property(nonatomic, weak) ViewRevealingVerticalPanHandler* panGestureHandler;

// Sets the location bar view controller, containing the omnibox.
- (void)setLocationBarViewController:(UIViewController*)locationBarView;

// Shows the animation when transitioning to a prerendered page.
- (void)showPrerenderingAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
