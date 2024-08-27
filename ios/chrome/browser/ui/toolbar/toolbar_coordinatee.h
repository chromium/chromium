// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATEE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATEE_H_

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"

@protocol PopupMenuUIUpdating;

// Defines a class being coordinated by a ToolbarCoordinating.
@protocol ToolbarCoordinatee<NewTabPageControllerDelegate, ToolbarCommands>

- (id<PopupMenuUIUpdating>)popupMenuUIUpdater;

- (UIViewController*)viewController;

- (void)showPrerenderingAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATEE_H_
