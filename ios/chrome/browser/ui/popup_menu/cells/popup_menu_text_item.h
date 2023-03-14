// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_TEXT_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"

// A non interactable textual item. The text wraps a leading image and
// description message.
@interface PopupMenuTextItem : TableViewItem <PopupMenuItem>

// The leading image name.
@property(nonatomic, copy) NSString* imageName;

// The string of the message.
@property(nonatomic, copy) NSString* message;

@end

// Associated cell for the PopupMenuTextItem.
@interface PopupMenuTextCell : TableViewCell

// Text label for the cell.
@property(nonatomic, strong) UILabel* messageLabel;

// The message of the item.
@property(nonatomic, strong) NSMutableAttributedString* messageAttributedString;

// After this is called, the cell is listening for the
// UIContentSizeCategoryDidChangeNotification notification and updates its font
// size to the new category.
- (void)registerForContentSizeUpdates;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_TEXT_ITEM_H_
