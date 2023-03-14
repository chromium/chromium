// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_UTIL_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_UTIL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/reading_list/reading_list_ui_distillation_status.h"

@protocol ReadingListListItem;

// Returns the a11y label to use for a reading list cell with `title`,
// `subtitle`, and `distillation_state`.
NSString* GetReadingListCellAccessibilityLabel(
    NSString* title,
    NSString* subtitle,
    ReadingListUIDistillationStatus distillation_status,
    BOOL showCloudSlashIcon);

// Returns the string to use to display the distillation date in reading list
// cells.  The date is in microseconds since Jan 1st 1970.
NSString* GetReadingListCellDistillationDateText(int64_t distillation_date);

// Returns whether `first` is equivalent to `second`.
BOOL AreReadingListListItemsEqual(id<ReadingListListItem> first,
                                  id<ReadingListListItem> second);

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_UTIL_H_
