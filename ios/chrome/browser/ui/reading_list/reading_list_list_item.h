// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/reading_list/reading_list_ui_distillation_status.h"

class GURL;
@class FaviconAttributes;
@class ReadingListListItemCustomActionFactory;

// Protocol used to supply reading list data to list items.
@protocol ReadingListListItem<NSObject>

// The title to display.
@property(nonatomic, copy) NSString* title;
// The URL of the Reading List entry.
@property(nonatomic, assign) GURL entryURL;
// The URL of the page presenting the favicon to display.
@property(nonatomic, assign) GURL faviconPageURL;
// Status of the offline version.
@property(nonatomic, assign) ReadingListUIDistillationStatus distillationState;
// The string that displays the estimated read time.
@property(nonatomic, copy) NSString* estimatedReadTimeText;
// The string that displays the distillation date.
@property(nonatomic, copy) NSString* distillationDateText;
// Whether the cloud slash icon should be shown.
@property(nonatomic, assign) BOOL showCloudSlashIcon;
// The custom action factory.
@property(nonatomic, weak)
    ReadingListListItemCustomActionFactory* customActionFactory;
// Attributes for favicon.
@property(nonatomic, strong) FaviconAttributes* attributes;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_H_
