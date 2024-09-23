// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_ITEM_DELEGATE_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_ITEM_DELEGATE_H_

@class ListItem;
@protocol HistoryEntryItemInterface;

// Delegate for HistoryEntryItem. Handles actions invoked as custom
// accessibility actions.
@protocol HistoryEntryItemDelegate
// Returns true if ListItem owner is in edit mode.
- (BOOL)isEditing;
// Called when custom accessibility action to delete the entry is invoked.
- (void)historyEntryItemDidRequestDelete:
    (ListItem<HistoryEntryItemInterface>*)item;
// Called when custom accessibility action to open the entry in a new tab is
// invoked.
- (void)historyEntryItemDidRequestOpenInNewTab:
    (ListItem<HistoryEntryItemInterface>*)item;
// Called when custom accessibility action to open the entry in a new incognito
// tab is invoked.
- (void)historyEntryItemDidRequestOpenInNewIncognitoTab:
    (ListItem<HistoryEntryItemInterface>*)item;
// Called when custom accessibility action to copy the entry's URL is invoked.
- (void)historyEntryItemDidRequestCopy:
    (ListItem<HistoryEntryItemInterface>*)item;
// Called when the view associated with the HistoryEntryItem should be updated.
- (void)historyEntryItemShouldUpdateView:
    (ListItem<HistoryEntryItemInterface>*)item;
@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_ITEM_DELEGATE_H_
