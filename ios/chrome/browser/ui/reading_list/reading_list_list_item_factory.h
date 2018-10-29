// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_FACTORY_H_

#import <UIKit/UIKit.h>

@class ListItem;
class ReadingListEntry;
@protocol ReadingListListItem;
@protocol ReadingListListItemAccessibilityDelegate;

// Factory object that produces ListItems for Reading List.
@interface ReadingListListItemFactory : NSObject

// The accessibility delegate to use for the created items.
@property(nonatomic, weak) id<ReadingListListItemAccessibilityDelegate>
    accessibilityDelegate;

// Factory method that provides a ListItem for the reading list.
- (ListItem<ReadingListListItem>*)cellItemForReadingListEntry:
    (const ReadingListEntry*)entry;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_FACTORY_H_
