// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TABLE_VIEW_ITEM_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TABLE_VIEW_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item.h"

// Table view item for representing a ReadingListEntry.
@interface ReadingListTableViewItem : TableViewItem<ReadingListListItem>
@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TABLE_VIEW_ITEM_H_
