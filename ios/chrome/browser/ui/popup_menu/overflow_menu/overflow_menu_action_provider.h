// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_ACTION_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_ACTION_PROVIDER_H_

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"

@class OverflowMenuAction;

// A class that provides `OverflowMenuAction`s for given action types.
@protocol OverflowMenuActionProvider <NSObject>

// The default base ranking of page actions (those that act on the current page
// state) currently supported.
- (ActionRanking)basePageActions;

// Returns the correct `OverflowMenuAction` for the corresponding
// `overflow_menu::ActionType` on the current page. Returns nil if the current
// page does not support the given `actionType`.
- (OverflowMenuAction*)actionForActionType:
    (overflow_menu::ActionType)actionType;

// Returns a representative `OverflowMenuAction` for the corresponding
// `overflow_menu::ActionType` to display to the user when customizing the order
// and show/hide state of the actions.
- (OverflowMenuAction*)customizationActionForActionType:
    (overflow_menu::ActionType)actionType;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_ACTION_PROVIDER_H_
