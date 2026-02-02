// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_BUTTONS_TOOLBAR_BUTTON_ACTIONS_HANDLER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_BUTTONS_TOOLBAR_BUTTON_ACTIONS_HANDLER_H_

#import <UIKit/UIKit.h>

@protocol ActivityServiceCommands;
@protocol BrowserCoordinatorCommands;
@protocol PopupMenuCommands;
@protocol SceneCommands;

class TabBasedIPHBrowserAgent;
class WebNavigationBrowserAgent;

// Handler for the actions associated with the different toolbar buttons.
@interface ToolbarButtonActionsHandler : NSObject

// Action Handlers
@property(nonatomic, weak) id<SceneCommands> sceneHandler;
@property(nonatomic, weak) id<ActivityServiceCommands> activityHandler;
@property(nonatomic, weak) id<PopupMenuCommands> menuHandler;
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorHandler;

@property(nonatomic, assign) WebNavigationBrowserAgent* navigationAgent;
@property(nonatomic, assign) TabBasedIPHBrowserAgent* tabBasedIPHAgent;

// Whether this handler is created in incognito.
@property(nonatomic, assign) BOOL incognito;

// Action when the back button is tapped.
- (void)backAction;

// Action when the forward button is tapped.
- (void)forwardAction;

// Action when there is a touch down on the tab grid button.
- (void)tabGridTouchDown;

// Action when there is a touch up on the tab grid button.
- (void)tabGridTouchUp;

// Action when the tools menu button is tapped.
- (void)toolsMenuAction;

// Action when the share button is tapped.
- (void)shareAction:(UIView*)sender;

// Action when the reload button is tapped.
- (void)reloadAction;

// Action when the stop button is tapped.
- (void)stopAction;

// Action when the new tab button is tapped.
- (void)newTabAction:(id)sender;

// Action when the button to cancel the omnibox focus is tapped.
- (void)cancelOmniboxFocusAction;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_BUTTONS_TOOLBAR_BUTTON_ACTIONS_HANDLER_H_
