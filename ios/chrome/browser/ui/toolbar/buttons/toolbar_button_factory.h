// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_FACTORY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_style.h"

@class ToolbarButton;
@class ToolbarButtonActionsHandler;
@class ToolbarButtonVisibilityConfiguration;
@class ToolbarTabGridButton;
@class ToolbarToolsMenuButton;
@class ToolbarConfiguration;

// ToolbarButton Factory protocol to create ToolbarButton objects with certain
// style and configuration, depending of the implementation.
// A dispatcher is used to send the commands associated with the buttons.
@interface ToolbarButtonFactory : NSObject

- (instancetype)initWithStyle:(ToolbarStyle)style NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, assign, readonly) ToolbarStyle style;
// Configuration object for styling. It is used by the factory to set the style
// of the buttons title.
@property(nonatomic, strong, readonly)
    ToolbarConfiguration* toolbarConfiguration;
// Handler for the actions.
@property(nonatomic, weak) ToolbarButtonActionsHandler* actionHandler;
// Configuration object for the visibility of the buttons.
@property(nonatomic, strong)
    ToolbarButtonVisibilityConfiguration* visibilityConfiguration;

// Back ToolbarButton.
- (ToolbarButton*)backButton;
// Forward ToolbarButton.
- (ToolbarButton*)forwardButton;
// Tab Grid ToolbarButton.
- (ToolbarTabGridButton*)tabGridButton;
// Tools Menu ToolbarButton.
- (ToolbarButton*)toolsMenuButton;
// Share ToolbarButton.
- (ToolbarButton*)shareButton;
// Reload ToolbarButton.
- (ToolbarButton*)reloadButton;
// Stop ToolbarButton.
- (ToolbarButton*)stopButton;
// ToolbarButton to create a new tab.
- (ToolbarButton*)openNewTabButton;
// Button to cancel the edit of the location bar.
- (UIButton*)cancelButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_FACTORY_H_
