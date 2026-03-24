// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_MENU_FACTORY_DELEGATE_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_MENU_FACTORY_DELEGATE_H_

namespace web {
class NavigationItem;
}  // namespace web
class TabGroup;

// Delegate protocol for receivers of toolbar button context menus.
@protocol ToolbarButtonMenuFactoryDelegate <NSObject>

// Navigates to the page associated with `item`.
- (void)navigateToPageForItem:(web::NavigationItem*)item;

// Creates a new tab for the current mode.
- (void)createNewTabFromView:(UIView*)sender;

// Creates a new tab group for the current mode.
- (void)createNewTabGroupFromView:(UIView*)sender;

// Adds a new tab to the current tab group.
- (void)addNewTabInCurrentTabGroup;

// Adds the current tab which is not in a tab group to `destinationGroup`.
- (void)addCurrentTabToGroup:(const TabGroup*)destinationGroup;

// Removes the current tab from its tab group.
- (void)removeCurrentTabFromGroup;

// Moves the current tab which is already in a tab group to `destinationGroup`.
- (void)moveCurrentTabToGroup:(const TabGroup*)destinationGroup;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_MENU_FACTORY_DELEGATE_H_
