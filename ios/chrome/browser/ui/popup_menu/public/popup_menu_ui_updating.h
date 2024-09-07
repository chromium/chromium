// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_UI_UPDATING_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_UI_UPDATING_H_

#import <UIKit/UIKit.h>

// Protocol for a class updating the UI to reflect the presentation of a popup
// menu.
@protocol PopupMenuUIUpdating
// Updates the UI for the presentation of an in-product help for the overflow
// menu.
- (void)updateUIForOverflowMenuIPHDisplayed;
// Updates the UI for the dismissal of an in-product help.
- (void)updateUIForIPHDismissed;
// Adds or removes blue dot to overflow menu button.
- (void)setOverflowMenuBlueDot:(BOOL)hasBlueDot;
@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_UI_UPDATING_H_
