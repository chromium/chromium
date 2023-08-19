// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OVERFLOW_MENU_CUSTOMIZATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OVERFLOW_MENU_CUSTOMIZATION_COMMANDS_H_

// Commands relating to showing or hiding the overflow menu customization
// process.
@protocol OverflowMenuCustomizationCommands

- (void)showActionCustomization;

- (void)hideActionCustomization;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OVERFLOW_MENU_CUSTOMIZATION_COMMANDS_H_
