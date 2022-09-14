// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MENU_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MENU_PROVIDER_H_

@class ReadingListListItem;

// Protocol for instances that will provide menus to ReadingList components.
@protocol ReadingListMenuProvider

// Creates a context menu configuration instance for the given `item` and it's
// corresponding `view`.
- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (id<ReadingListListItem>)item
                                                      withView:(UIView*)view;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MENU_PROVIDER_H_
