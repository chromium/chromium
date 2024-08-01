// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRIES_STATUS_ITEM_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRIES_STATUS_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Model item for HistoryEntriesStatusCell. Contains information on what type of
// History entries are being displayed.
@interface HistoryEntriesStatusItem : TableViewItem
@end

// Cell that displays a HistoryEntriesStatusItem.
@interface HistoryEntriesStatusCell : TableViewCell
@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRIES_STATUS_ITEM_H_
