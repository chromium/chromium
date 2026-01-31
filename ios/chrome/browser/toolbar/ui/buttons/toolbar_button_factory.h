// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_FACTORY_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_FACTORY_H_

#import <UIKit/UIKit.h>

@class ToolbarButton;

// Factory for creating toolbar buttons.
@interface ToolbarButtonFactory : NSObject

// Creates a back button.
- (ToolbarButton*)makeBackButton;

// Creates a forward button.
- (ToolbarButton*)makeForwardButton;

// Creates a reload button.
- (ToolbarButton*)makeReloadButton;

// Creates a stop button.
- (ToolbarButton*)makeStopButton;

// Creates a share button.
- (ToolbarButton*)makeShareButton;

// Creates a tab grid button.
- (ToolbarButton*)makeTabGridButton;

// Creates an assistant button.
- (ToolbarButton*)makeAssistantButton;

// Creates a tools menu button.
- (ToolbarButton*)makeToolsMenuButton;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_FACTORY_H_
