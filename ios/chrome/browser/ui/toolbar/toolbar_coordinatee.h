// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATEE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATEE_H_

#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller_delegate.h"

@protocol PopupMenuUIUpdating;

// Defines a class being coordinated by a ToolbarCoordinating.
@protocol ToolbarCoordinatee<NewTabPageControllerDelegate, ToolbarCommands>

- (id<PopupMenuUIUpdating>)popupMenuUIUpdater;

- (UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATEE_H_
