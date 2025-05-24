// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_INCOGNITO_GRID_COMMANDS_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_INCOGNITO_GRID_COMMANDS_H_

#import <UIKit/UIKit.h>

// Commands issued to a model backing an Incognito grid UI.
// TODO(crbug.com/414766261): Move to GridCommands once GridCommands is a real
// commands protocol (i.e. when handled by the grid coordinator, instead of the
// mediator, and when mutator-related methods are moved to a mutator protocol).
@protocol IncognitoGridCommands

// Tells the receiver to dismiss any modal UI that the Incognito grid could have
// necessitated.
- (void)dismissIncognitoGridModals;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_INCOGNITO_GRID_COMMANDS_H_
