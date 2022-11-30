// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_CONSUMER_H_

#import <Foundation/Foundation.h>

@protocol PopupMenuItem;
@class TableViewItem;

// Consumer protocol for the popup menu.
@protocol PopupMenuConsumer

// Item to be highlighted. Nil if no item should be highlighted. Must be set
// after the popup menu items.
@property(nonatomic, weak) TableViewItem<PopupMenuItem>* itemToHighlight;

// Sets the `items` to be displayed by this Consumer. Removes all the currently
// presented items.
- (void)setPopupMenuItems:
    (NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>*)items;
// Notifies the consumer that `items` have changed.
- (void)itemsHaveChanged:(NSArray<TableViewItem<PopupMenuItem>*>*)items;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_CONSUMER_H_
