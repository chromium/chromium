// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_BUTTONS_LEGACY_TOOLBAR_BUTTON_FACTORY_H_
#define IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_BUTTONS_LEGACY_TOOLBAR_BUTTON_FACTORY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_style.h"

@protocol BWGCommands;
@class LegacyToolbarButton;
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

// LegacyToolbarButton Factory protocol to create LegacyToolbarButton objects
// with certain style and configuration, depending of the implementation. A
// dispatcher is used to send the commands associated with the buttons.
@interface LegacyToolbarButtonFactory : NSObject

- (instancetype)initWithStyle:(ToolbarStyle)style NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, assign, readonly) ToolbarStyle style;
// Configuration object for styling. It is used by the factory to set the style
// of the buttons title.
@property(nonatomic, strong, readonly)
    ToolbarConfiguration* toolbarConfiguration;
// Handler for the actions.
@property(nonatomic, weak) ToolbarButtonActionsHandler* actionHandler;
// Handler for gemini commands.
@property(nonatomic, weak) id<BWGCommands> geminiHandler;
// Configuration object for the visibility of the buttons.
@property(nonatomic, strong)
    ToolbarButtonVisibilityConfiguration* visibilityConfiguration;

// Back LegacyToolbarButton.
- (LegacyToolbarButton*)backButton;
// Forward LegacyToolbarButton.
- (LegacyToolbarButton*)forwardButton;
// Tab Grid LegacyToolbarButton.
- (ToolbarTabGridButton*)tabGridButton;
// Tools Menu LegacyToolbarButton.
- (LegacyToolbarButton*)toolsMenuButton;
// Share LegacyToolbarButton.
- (LegacyToolbarButton*)shareButton;
// Reload LegacyToolbarButton.
- (LegacyToolbarButton*)reloadButton;
// Stop LegacyToolbarButton.
- (LegacyToolbarButton*)stopButton;
// LegacyToolbarButton to create a new tab.
- (LegacyToolbarButton*)openNewTabButton;
// Button to cancel the edit of the location bar.
- (UIButton*)cancelButton;
- (UIButton*)cancelButtonWithStyle:(ToolbarCancelButtonStyle)style;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_BUTTONS_LEGACY_TOOLBAR_BUTTON_FACTORY_H_
