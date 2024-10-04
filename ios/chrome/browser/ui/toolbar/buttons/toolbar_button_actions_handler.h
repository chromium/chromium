// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_ACTIONS_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_ACTIONS_HANDLER_H_

#import <Foundation/Foundation.h>

@protocol ApplicationCommands;
@protocol ActivityServiceCommands;
@protocol PopupMenuCommands;
@protocol OmniboxCommands;

class TabBasedIPHBrowserAgent;
class WebNavigationBrowserAgent;

// Handler for the actions associated with the different toolbar buttons.
@interface ToolbarButtonActionsHandler : NSObject

// Action Handlers
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;
@property(nonatomic, weak) id<ActivityServiceCommands> activityHandler;
@property(nonatomic, weak) id<PopupMenuCommands> menuHandler;
@property(nonatomic, weak) id<OmniboxCommands> omniboxHandler;

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
- (void)shareAction;

// Action when the reload button is tapped.
- (void)reloadAction;

// Action when the stop button is tapped.
- (void)stopAction;

// Action when the new tab button is tapped.
- (void)newTabAction:(id)sender;

// Action when the button to cancel the omnibox focus is tapped.
- (void)cancelOmniboxFocusAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_ACTIONS_HANDLER_H_
