// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_ACCESSIBILITY_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_ACCESSIBILITY_DELEGATE_H_

@protocol ReadingListListItem;

// Protocol used to implement custom accessibility actions for cells set up by
// ReadingListListItems.
@protocol ReadingListListItemAccessibilityDelegate

// Returns whether the entry is read.
- (BOOL)isItemRead:(id<ReadingListListItem>)item;

- (void)openItemInNewTab:(id<ReadingListListItem>)item;
- (void)openItemInNewIncognitoTab:(id<ReadingListListItem>)item;
- (void)openItemOffline:(id<ReadingListListItem>)item;
- (void)markItemRead:(id<ReadingListListItem>)item;
- (void)markItemUnread:(id<ReadingListListItem>)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_ACCESSIBILITY_DELEGATE_H_
