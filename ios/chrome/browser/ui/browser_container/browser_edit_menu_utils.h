// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_EDIT_MENU_UTILS_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_EDIT_MENU_UTILS_H_

#import <UIKit/UIKit.h>

namespace edit_menu {

// Adds an element at the end of the Chrome menu.
// Chrome menu is added between format menu and lookup menu.
void AddElementToChromeMenu(id<UIMenuBuilder> builder, UIMenuElement* element);

}  // namespace edit_menu

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_EDIT_MENU_UTILS_H_
