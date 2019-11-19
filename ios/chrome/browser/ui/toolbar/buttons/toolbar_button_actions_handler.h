// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_ACTIONS_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_ACTIONS_HANDLER_H_

#import <Foundation/Foundation.h>

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol OmniboxFocuser;

// Handler for the actions associated with the different toolbar buttons.
@interface ToolbarButtonActionsHandler : NSObject

// Dispatcher for the actions.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, OmniboxFocuser>
        dispatcher;

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

// Action when the bookmark button is tapped.
- (void)bookmarkAction;

// Action when the search button is tapped.
- (void)searchAction:(id)sender;

// Action when the button to cancel the omnibox focus is tapped.
- (void)cancelOmniboxFocusAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_ACTIONS_HANDLER_H_
