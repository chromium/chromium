// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/history_entry_inserter.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/history/ui_bundled/history_entry_item_interface.h"
#import "ios/chrome/browser/history/ui_bundled/history_util.h"
#import "url/gurl.h"

@interface HistoryEntryInserter () {
  // ListModel in which to insert history entries.
  ListModel* _listModel;
  // The index of the first section to contain history entries.
  NSInteger _firstSectionIndex;
  // Number of assigned section identifiers.
  NSInteger _sectionIdentifierCount;
  // Sorted set of dates that have history entries.
  NSMutableOrderedSet* _dates;
  // Mapping from dates to section identifiers.
  NSMutableDictionary* _sectionIdentifiers;
}

@end

@implementation HistoryEntryInserter
@synthesize delegate = _delegate;

- (instancetype)initWithModel:(ListModel*)listModel {
  if ((self = [super init])) {
    _listModel = listModel;
    _firstSectionIndex = [listModel numberOfSections];
    _dates = [[NSMutableOrderedSet alloc] init];
    _sectionIdentifiers = [NSMutableDictionary dictionary];
  }
  return self;
}

- (void)insertHistoryEntryItem:(ListItem<HistoryEntryItemInterface>*)item {
  NSInteger sectionIdentifier =
      [self sectionIdentifierForTimestamp:item.timestamp];

  NSComparator objectComparator = ^(id obj1, id obj2) {
    ListItem<HistoryEntryItemInterface>* firstObject =
        base::apple::ObjCCastStrict<ListItem<HistoryEntryItemInterface>>(obj1);
    ListItem<HistoryEntryItemInterface>* secondObject =
        base::apple::ObjCCastStrict<ListItem<HistoryEntryItemInterface>>(obj2);
    if ([firstObject isEqual:secondObject])
      return NSOrderedSame;

    // History entries are ordered from most to least recent.
    if (firstObject.timestamp > secondObject.timestamp)
      return NSOrderedAscending;
    if (firstObject.timestamp < secondObject.timestamp)
      return NSOrderedDescending;
    return firstObject.URL < secondObject.URL ? NSOrderedAscending
                                              : NSOrderedDescending;
  };

  NSArray* items = [_listModel itemsInSectionWithIdentifier:sectionIdentifier];
  NSRange range = NSMakeRange(0, [items count]);
  // If the object is not already in the section, insert it.
  if ([items indexOfObject:item
             inSortedRange:range
                   options:NSBinarySearchingFirstEqual
           usingComparator:objectComparator] == NSNotFound) {
    // Insert the object at the appropriate index to keep the section sorted.
    NSUInteger index = [items indexOfObject:item
                              inSortedRange:range
                                    options:NSBinarySearchingInsertionIndex
                            usingComparator:objectComparator];

    // Calculate the new tableView indexPath row before inserting into the
    // model. No matter where in the model the item is inserted, a new row will
    // be created for the tableView. For this reason, make sure to insert a new
    // index into the tableView after the item has been inserted into the model.
    NSInteger section =
        [_listModel sectionForSectionIdentifier:sectionIdentifier];
    NSInteger tableViewRow = [_listModel numberOfItemsInSection:section];
    NSIndexPath* tableIndexPath =
        [NSIndexPath indexPathForRow:tableViewRow inSection:section];

    [_listModel insertItem:item
        inSectionWithIdentifier:sectionIdentifier
                        atIndex:index];
    [self.delegate historyEntryInserter:self
               didInsertItemAtIndexPath:tableIndexPath];
  }
}

- (NSUInteger)sectionIdentifierForTimestamp:(base::Time)timestamp {
  base::TimeDelta timeDelta =
      timestamp.LocalMidnight() - base::Time::UnixEpoch();
  NSDate* date = [NSDate dateWithTimeIntervalSince1970:timeDelta.InSeconds()];

  NSInteger sectionIdentifier =
      [[_sectionIdentifiers objectForKey:date] integerValue];
  // If there is a section identifier for the date, return it.
  if (sectionIdentifier) {
    return sectionIdentifier;
  }

  // Get the next section identifier, and add a section for date.
  sectionIdentifier =
      kSectionIdentifierEnumZero + _firstSectionIndex + _sectionIdentifierCount;
  ++_sectionIdentifierCount;
  [_sectionIdentifiers setObject:@(sectionIdentifier) forKey:date];

  NSComparator comparator = ^(id obj1, id obj2) {
    // Dates are ordered from most to least recent.
    return [obj2 compare:obj1];
  };
  NSUInteger index = [_dates indexOfObject:date
                             inSortedRange:NSMakeRange(0, [_dates count])
                                   options:NSBinarySearchingInsertionIndex
                           usingComparator:comparator];
  [_dates insertObject:date atIndex:index];
  NSInteger insertionIndex = _firstSectionIndex + index;
  [_listModel insertSectionWithIdentifier:sectionIdentifier
                                  atIndex:insertionIndex];

    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc] initWithType:kItemTypeEnumZero];
    header.text =
        base::SysUTF16ToNSString(history::GetRelativeDateLocalized(timestamp));
    [_listModel setHeader:header forSectionWithIdentifier:sectionIdentifier];

  [self.delegate historyEntryInserter:self
              didInsertSectionAtIndex:insertionIndex];
  return sectionIdentifier;
}

- (void)removeSection:(NSInteger)sectionIndex {
  NSUInteger sectionIdentifier =
      [_listModel sectionIdentifierForSectionIndex:sectionIndex];

  // Sections should not be removed unless there are no items in that section.
  DCHECK(![[_listModel itemsInSectionWithIdentifier:sectionIdentifier] count]);
  [_listModel removeSectionWithIdentifier:sectionIdentifier];

  NSEnumerator* dateEnumerator = [_sectionIdentifiers keyEnumerator];
  NSDate* date = nil;
  while ((date = [dateEnumerator nextObject])) {
    if ([[_sectionIdentifiers objectForKey:date] unsignedIntegerValue] ==
        sectionIdentifier) {
      [_sectionIdentifiers removeObjectForKey:date];
      [_dates removeObject:date];
      break;
    }
  }
  [self.delegate historyEntryInserter:self
              didRemoveSectionAtIndex:sectionIndex];
}

@end
