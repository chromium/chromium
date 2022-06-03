// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_UI_UPDATING_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_UI_UPDATING_H_

#import <UIKit/UIKit.h>

// Type of popup menus.
typedef NS_ENUM(NSInteger, PopupMenuType) {
  PopupMenuTypeToolsMenu,
  PopupMenuTypeNavigationBackward,
  PopupMenuTypeNavigationForward,
  PopupMenuTypeTabGrid,
  PopupMenuTypeNewTab,
  PopupMenuTypeTabStripTabGrid,
};

// Protocol for a class updating the UI to reflect the presentation of a popup
// menu.
@protocol PopupMenuUIUpdating
// Updates the UI for the presentation of the popup menu of type |popupType|.
- (void)updateUIForMenuDisplayed:(PopupMenuType)popupType;
// Update the UI for the dismissal of the popup menu.
- (void)updateUIForMenuDismissed;
@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_UI_UPDATING_H_
