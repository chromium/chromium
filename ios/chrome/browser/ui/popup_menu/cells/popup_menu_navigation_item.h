// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_NAVIGATION_ITEM_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_NAVIGATION_ITEM_H_

#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

namespace web {
class NavigationItem;
}  // namespace web

// Item used to display an item for a navigation menu.
@interface PopupMenuNavigationItem : TableViewItem<PopupMenuItem>
// Title of the navigation item.
@property(nonatomic, copy) NSString* title;
// Favicon to be displayed. Set to nil to display the default favicon.
@property(nonatomic, strong) UIImage* favicon;
// Item used to navigate in the history.
@property(nonatomic, assign) web::NavigationItem* navigationItem;
@end

// Associated cell for a PopupMenuNavigationItem.
@interface PopupMenuNavigationCell : TableViewCell

- (void)setTitle:(NSString*)title;
- (void)setFavicon:(UIImage*)favicon;

// After this is called, the cell is listening for the
// UIContentSizeCategoryDidChangeNotification notification and updates its font
// size to the new category.
- (void)registerForContentSizeUpdates;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_NAVIGATION_ITEM_H_
