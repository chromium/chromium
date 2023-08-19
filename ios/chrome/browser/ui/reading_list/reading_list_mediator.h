// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/memory/scoped_refptr.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_data_source.h"

class FaviconLoader;
class GURL;
class ReadingListEntry;
@class ReadingListListItemFactory;
class ReadingListModel;
namespace syncer {
class SyncService;
}

// Mediator between the Model and the UI.
@interface ReadingListMediator : NSObject<ReadingListDataSource>

- (nullable instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithModel:(nonnull ReadingListModel*)model
                           syncService:(nonnull syncer::SyncService*)syncService
                         faviconLoader:(nonnull FaviconLoader*)faviconLoader
                       listItemFactory:
                           (nonnull ReadingListListItemFactory*)itemFactory
    NS_DESIGNATED_INITIALIZER;

// Returns the entry corresponding to the `item`. The item should be of type
// ReadingListCollectionViewItem. Returns nullptr if there is no corresponding
// entry.
- (scoped_refptr<const ReadingListEntry>)entryFromItem:
    (nonnull id<ReadingListListItem>)item;

// Marks the entry with `URL` as read.
- (void)markEntryRead:(const GURL&)URL;

// Disconnects the mediator and clear internal dependencies.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MEDIATOR_H_
