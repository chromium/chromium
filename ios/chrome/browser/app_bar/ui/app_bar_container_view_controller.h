// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"

@class AppBarViewController;
@class BrowserLayoutState;
@class IncognitoState;
@class SceneLayoutState;

// View controller for the App Bar container. This is the view controller in
// charge of making sure the app stays at the physical bottom of the screen.
// To do this, it is covering the whole screen and then manage the rotation
// itself. It needs to be centered in the window to work.
@interface AppBarContainerViewController
    : UIViewController <FullscreenBrowserAgentObserving, FullscreenUIElement>

// The regular browser layout state.
@property(nonatomic, weak) BrowserLayoutState* regularBrowserLayoutState;

// The incognito browser layout state.
@property(nonatomic, weak) BrowserLayoutState* incognitoBrowserLayoutState;

// The incognito state.
@property(nonatomic, weak) IncognitoState* incognitoState;

// The scene layout state.
@property(nonatomic, weak) SceneLayoutState* sceneLayoutState;

// Sets the App Bar view controller to be contained.
- (void)setAppBar:(AppBarViewController*)appBar;

// Updates the corner radius of the app bar cutout to match the container
// layout. Clamps the radius between the default and max corner radius.
- (void)updateCutoutRadius:(CGFloat)cutoutRadius;

// Returns the current portrait height of the App Bar, taking into account
// whether the Gemini floaty is currently invoked.
- (CGFloat)appBarHeightPortrait;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_CONTROLLER_H_
