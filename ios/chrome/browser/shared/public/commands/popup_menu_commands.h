// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_POPUP_MENU_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_POPUP_MENU_COMMANDS_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}

// Commands for the popup menu.
@protocol PopupMenuCommands

// Shows the tools menu.
- (void)showToolsMenuPopup;
// Dismisses the currently presented popup.
- (void)dismissPopupMenuAnimated:(BOOL)animated;
// Shows a snackbar that allows the user to UNDO its pin/unpin action.
- (void)showSnackbarForPinnedState:(BOOL)pinnedState
                          webState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_POPUP_MENU_COMMANDS_H_
