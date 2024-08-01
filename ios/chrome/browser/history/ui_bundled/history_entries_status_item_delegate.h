// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRIES_STATUS_ITEM_DELEGATE_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRIES_STATUS_ITEM_DELEGATE_H_

class GURL;
@class LegacyHistoryEntriesStatusItem;

// Delegate HistoryEntriesStatusItem. Handles link taps on
// HistoryEntriesStatusCell.
@protocol HistoryEntriesStatusItemDelegate
// Called when a link is pressed on a HistoryEntriesStatusCell.
- (void)historyEntriesStatusItem:(LegacyHistoryEntriesStatusItem*)item
               didRequestOpenURL:(const GURL&)URL;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRIES_STATUS_ITEM_DELEGATE_H_
