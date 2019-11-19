// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_CELLS_TRANSLATE_POPUP_MENU_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_CELLS_TRANSLATE_POPUP_MENU_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// Item used for the translate infobar's popup menus.
@interface TranslatePopupMenuItem : TableViewItem <PopupMenuItem>

// Title of the item.
@property(nonatomic, copy) NSString* title;

// Whether the item is selected.
@property(nonatomic, getter=isSelected) BOOL selected;

@end

// Associated cell for a TranslatePopupMenuItem.
@interface TranslatePopupMenuCell : TableViewCell

- (void)setTitle:(NSString*)title;

// Whether the cell will display a trailing checkmark or not.
- (void)setCheckmark:(BOOL)checkmark;

// After this is called, the cell is listening for the
// UIContentSizeCategoryDidChangeNotification notification and updates its font
// size to the new category.
- (void)registerForContentSizeUpdates;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_CELLS_TRANSLATE_POPUP_MENU_ITEM_H_
