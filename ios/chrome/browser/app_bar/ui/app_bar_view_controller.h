// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"

@protocol AppBarMutator;
@class LayoutGuideCenter;
@protocol SceneCommands;
@protocol TabGridCommands;
@class LayoutState;

// View controller for the App Bar.
@interface AppBarViewController
    : UIViewController <AppBarConsumer,
                        FullscreenUIElement,
                        FullscreenBrowserAgentObserving>

// The layout state.
@property(nonatomic, weak) LayoutState* layoutState;

// The mutator.
@property(nonatomic, weak) id<AppBarMutator> mutator;

// This view controller's LayoutGuideCenter.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Command handler for the Scene commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

// Tab Grid handler.
@property(nonatomic, weak) id<TabGridCommands> tabGridHandler;

// Updates the App Bar's subviews for a given rotation angle.
- (void)updateForAngle:(CGFloat)angle;

// Unhides the spotlight anchor view if `shouldShow`.
- (void)toggleSpotlightView:(BOOL)shouldShow;

// Shows the blue-ish background with a circular gradient.
// If `centered` is YES, the gradient is centered. Otherwise, it is left-bottom
// aligned.
- (void)showIPHBackgroundWithCentering:(BOOL)centered;

// Hides the blue-ish background.
- (void)hideIPHBackground;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_CONTROLLER_H_
