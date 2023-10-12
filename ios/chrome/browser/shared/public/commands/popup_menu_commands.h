// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_POPUP_MENU_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_POPUP_MENU_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands for the popup menu.
@protocol PopupMenuCommands

// Shows the tools menu.
- (void)showToolsMenuPopup;
// Dismisses the currently presented popup.
- (void)dismissPopupMenuAnimated:(BOOL)animated;
// Adjusts the popup's size when the containing view's size changes.
- (void)adjustPopupSize;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_POPUP_MENU_COMMANDS_H_
