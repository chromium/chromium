// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_UPDATER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_UPDATER_H_

#import <Foundation/Foundation.h>

@protocol ReadingListListItem;

// Typedef for a block taking a ReadingListListItem as parameter and returning
// nothing.
typedef void (^ReadingListListItemUpdater)(id<ReadingListListItem> item);

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_UPDATER_H_
