// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_INSERTER_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_INSERTER_H_

#import <Foundation/Foundation.h>

namespace base {
class Time;
}
@class ListModel;
@class HistoryEntryInserter;
@class ListItem;
@protocol HistoryEntryItemInterface;

// Delegate for HistoryEntryInserter. Provides callbacks for completion of item
// and section insertion and deletion.
@protocol HistoryEntryInserterDelegate<NSObject>
// Invoked when the inserter has finished inserting an item.
- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
    didInsertItemAtIndexPath:(NSIndexPath*)indexPath;
// Invoked when the inserter has finished inserting a section.
- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
     didInsertSectionAtIndex:(NSInteger)sectionIndex;
// Invoked when the inserter has finished removing a section.
- (void)historyEntryInserter:(HistoryEntryInserter*)inserter
     didRemoveSectionAtIndex:(NSInteger)sectionIndex;
@end

// Object for ensuring history entry items are kept in order as they are added
// to the ListModel.
@interface HistoryEntryInserter : NSObject

// Delegate for the HistoryEntryInserter. Receives callbacks upon item and
// section insertion and removal.
@property(nonatomic, weak) id<HistoryEntryInserterDelegate> delegate;

// Designated initializer for HistoryEntryInserter. listModel is the
// model into which entries are inserted. Sections for history entries are
// appended to the model. Sections already in the model at initialization
// of the inserter should not be removed, and sections should not be added
// except by the inserter. Duplicate entries are not inserted.
- (instancetype)initWithModel:(ListModel*)listModel NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Inserts a history entry into the model at the correct sorted index path.
// History entries in the model are sorted from most to least recent, and
// grouped into section by date. Duplicate entries are not inserted. Invokes
// delegate callback when insertion is complete.
- (void)insertHistoryEntryItem:(ListItem<HistoryEntryItemInterface>*)item;

// Returns section identifier for provided timestamp. Adds section for date if
// not found, and invokes delegate callback.
- (NSUInteger)sectionIdentifierForTimestamp:(base::Time)timestamp;

// Removes section at `sectionIndex`, and invokes delegate callback when removal
// is complete.
- (void)removeSection:(NSInteger)sectionIndex;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_ENTRY_INSERTER_H_
