// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_BUTTONS_TOOLBAR_BUTTON_FACTORY_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_BUTTONS_TOOLBAR_BUTTON_FACTORY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_style.h"

@class ToolbarButton;
@class ToolbarButtonActionsHandler;
@class ToolbarButtonVisibilityConfiguration;
@class ToolbarConfiguration;
@class ToolbarTabGridButton;
@class ToolbarToolsMenuButton;

// The possible styles for the cancel buttons.
enum class ToolbarCancelButtonStyle {
  // Present the cancel button as a label.
  kCancelLabel = 0,
  // Present the cancel button as X circle.
  kXCircle = 2,
};

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
// ToolbarButton for the diamond prototype.
- (ToolbarButton*)diamondPrototypeButton;
// Button to cancel the edit of the location bar.
- (UIButton*)cancelButton;
- (UIButton*)cancelButtonWithStyle:(ToolbarCancelButtonStyle)style;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_BUTTONS_TOOLBAR_BUTTON_FACTORY_H_
