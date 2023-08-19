// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_VISIBILITY_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_VISIBILITY_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_component_options.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"

// Toolbar button configuration object giving access to visibility mask for each
// button.
@interface ToolbarButtonVisibilityConfiguration : NSObject

// Init the toolbar configuration with the desired `type`.
- (instancetype)initWithType:(ToolbarType)type NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Type of this configuration.
@property(nonatomic, assign) ToolbarType type;

// Configuration for the different buttons.
@property(nonatomic, readonly) ToolbarComponentVisibility backButtonVisibility;
// Configuration for the forward button displayed in the leading part of the
// toolbar.
@property(nonatomic, readonly)
    ToolbarComponentVisibility forwardButtonVisibility;
@property(nonatomic, readonly)
    ToolbarComponentVisibility tabGridButtonVisibility;
@property(nonatomic, readonly)
    ToolbarComponentVisibility toolsMenuButtonVisibility;
@property(nonatomic, readonly) ToolbarComponentVisibility shareButtonVisibility;
@property(nonatomic, readonly)
    ToolbarComponentVisibility reloadButtonVisibility;
@property(nonatomic, readonly) ToolbarComponentVisibility stopButtonVisibility;
@property(nonatomic, readonly)
    ToolbarComponentVisibility voiceSearchButtonVisibility;
@property(nonatomic, readonly)
    ToolbarComponentVisibility contractButtonVisibility;
@property(nonatomic, readonly)
    ToolbarComponentVisibility newTabButtonVisibility;
@property(nonatomic, readonly)
    ToolbarComponentVisibility locationBarLeadingButtonVisibility;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_VISIBILITY_CONFIGURATION_H_
