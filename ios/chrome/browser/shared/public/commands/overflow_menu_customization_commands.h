// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OVERFLOW_MENU_CUSTOMIZATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OVERFLOW_MENU_CUSTOMIZATION_COMMANDS_H_

namespace overflow_menu {
enum class ActionType;
}

// Commands relating to showing or hiding the overflow menu customization
// process.
@protocol OverflowMenuCustomizationCommands

- (void)showMenuCustomization;

- (void)showMenuCustomizationFromActionType:
    (overflow_menu::ActionType)actionType;

- (void)hideMenuCustomization;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OVERFLOW_MENU_CUSTOMIZATION_COMMANDS_H_
