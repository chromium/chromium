// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol UrlLoader;
@protocol OmniboxFocuser;

// The coordinator for the shortcuts.
// Shortcuts are the tiles displayed in the omnibox in the zero state.
@interface ShortcutsCoordinator : ChromeCoordinator

// The view controller managed by this coordinator.
@property(nonatomic, strong, readonly) UIViewController* viewController;

@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, UrlLoader, OmniboxFocuser>
        dispatcher;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_COORDINATOR_H_
