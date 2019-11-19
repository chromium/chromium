// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_mediator.h"

#include <algorithm>

#import "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#include "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_data_sink.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_util.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/favicon/favicon_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Sorter function that orders ReadingListEntries by their update time.
bool EntrySorter(const ReadingListEntry* rhs, const ReadingListEntry* lhs) {
  return rhs->UpdateTime() > lhs->UpdateTime();
}
// Desired width and height of favicon.
const CGFloat kFaviconWidthHeight = 24;
// Minimum favicon size to retrieve.
const CGFloat kFaviconMinWidthHeight = 16;

}  // namespace

@interface ReadingListMediator ()<ReadingListModelBridgeObserver> {
  std::unique_ptr<ReadingListModelBridge> _modelBridge;
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate> _batchToken;
}

// The model passed on initialization.
@property(nonatomic, assign) ReadingListModel* model;

// Whether the consumer should be notified of model changes.
@property(nonatomic, assign) BOOL shouldMonitorModel;

// The ListItem factory passed on initialization.
@property(nonatomic, strong) ReadingListListItemFactory* itemFactory;

// Favicon Service used for UIRefresh Collections.
@property(nonatomic, assign, readonly) FaviconLoader* faviconLoader;

@end

@implementation ReadingListMediator

@synthesize dataSink = _dataSink;
@synthesize model = _model;
@synthesize shouldMonitorModel = _shouldMonitorModel;
@synthesize itemFactory = _itemFactory;
@synthesize faviconLoader = _faviconLoader;

#pragma mark - Public

- (instancetype)initWithModel:(ReadingListModel*)model
                faviconLoader:(nonnull FaviconLoader*)faviconLoader
              listItemFactory:(ReadingListListItemFactory*)itemFactory {
  self = [super init];
  if (self) {
    _model = model;
    _itemFactory = itemFactory;
    _shouldMonitorModel = YES;
    _faviconLoader = faviconLoader;

    // This triggers the callback method. Should be created last.
    _modelBridge.reset(new ReadingListModelBridge(self, model));
  }
  return self;
}

- (const ReadingListEntry*)entryFromItem:(id<ReadingListListItem>)item {
  return self.model->GetEntryByURL(item.entryURL);
}

- (void)markEntryRead:(const GURL&)URL {
  self.model->SetReadStatus(URL, true);
}

#pragma mark - ReadingListDataSource

- (BOOL)isItemRead:(id<ReadingListListItem>)item {
  const ReadingListEntry* readingListEntry =
      self.model->GetEntryByURL(item.entryURL);

  if (!readingListEntry) {
    return NO;
  }

  return readingListEntry->IsRead();
}

- (void)dataSinkWillBeDismissed {
  self.model->MarkAllSeen();
  // Reset data sink to prevent further model update notifications.
  self.dataSink = nil;
}

- (void)setReadStatus:(BOOL)read forItem:(id<ReadingListListItem>)item {
  self.model->SetReadStatus(item.entryURL, read);
}

- (const ReadingListEntry*)entryWithURL:(const GURL&)URL {
  return self.model->GetEntryByURL(URL);
}

- (void)removeEntryFromItem:(id<ReadingListListItem>)item {
  [self logDeletionOfItem:item];
  self.model->RemoveEntryByURL(item.entryURL);
}

- (void)fillReadItems:(NSMutableArray<id<ReadingListListItem>>*)readArray
          unreadItems:(NSMutableArray<id<ReadingListListItem>>*)unreadArray {
  std::vector<const ReadingListEntry*> readEntries;
  std::vector<const ReadingListEntry*> unreadEntries;

  for (const auto& url : self.model->Keys()) {
    const ReadingListEntry* entry = self.model->GetEntryByURL(url);
    DCHECK(entry);
    if (entry->IsRead()) {
      readEntries.push_back(entry);
    } else {
      unreadEntries.push_back(entry);
    }
  }

  std::sort(readEntries.begin(), readEntries.end(), EntrySorter);
  std::sort(unreadEntries.begin(), unreadEntries.end(), EntrySorter);

  for (const ReadingListEntry* entry : readEntries) {
    [readArray addObject:[self.itemFactory cellItemForReadingListEntry:entry]];
  }

  for (const ReadingListEntry* entry : unreadEntries) {
    [unreadArray
        addObject:[self.itemFactory cellItemForReadingListEntry:entry]];
  }

  DCHECK(self.model->Keys().size() == [readArray count] + [unreadArray count]);
}

- (void)fetchFaviconForItem:(id<ReadingListListItem>)item {
  __weak id<ReadingListListItem> weakItem = item;
  __weak ReadingListMediator* weakSelf = self;
  void (^completionBlock)(FaviconAttributes* attributes) =
      ^(FaviconAttributes* attributes) {
        id<ReadingListListItem> strongItem = weakItem;
        ReadingListMediator* strongSelf = weakSelf;
        if (!strongSelf || !strongItem) {
          return;
        }
        strongItem.attributes = attributes;

        [strongSelf.dataSink itemHasChangedAfterDelay:strongItem];
      };
  self.faviconLoader->FaviconForPageUrl(
      item.faviconPageURL, kFaviconWidthHeight, kFaviconMinWidthHeight,
      /*fallback_to_google_server=*/false, completionBlock);
}

- (void)beginBatchUpdates {
  self.shouldMonitorModel = NO;
  _batchToken = self.model->BeginBatchUpdates();
}

- (void)endBatchUpdates {
  _batchToken.reset();
  self.shouldMonitorModel = YES;
}

#pragma mark - Properties

- (void)setDataSink:(id<ReadingListDataSink>)dataSink {
  _dataSink = dataSink;
  if (self.model->loaded()) {
    [dataSink dataSourceReady:self];
  }
}

- (BOOL)isReady {
  return self.model->loaded();
}

- (BOOL)hasElements {
  return self.model->size() > 0;
}

- (BOOL)hasReadElements {
  return self.model->size() != self.model->unread_size();
}

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  UMA_HISTOGRAM_COUNTS_1000("ReadingList.Unread.Number", model->unread_size());
  UMA_HISTOGRAM_COUNTS_1000("ReadingList.Read.Number",
                            model->size() - model->unread_size());
  [self.dataSink dataSourceReady:self];
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
  if (!self.shouldMonitorModel) {
    return;
  }

  // Ignore single element updates when the data source is doing batch updates.
  if (self.model->IsPerformingBatchUpdates()) {
    return;
  }

  if ([self hasDataSourceChanged])
    [self.dataSink dataSourceChanged];
}

- (void)readingListModelCompletedBatchUpdates:(const ReadingListModel*)model {
  if (!self.shouldMonitorModel) {
    return;
  }

  if ([self hasDataSourceChanged])
    [self.dataSink dataSourceChanged];
}

#pragma mark - Private

// Whether the data source has changed.
- (BOOL)hasDataSourceChanged {
  NSMutableArray<id<ReadingListListItem>>* readArray = [NSMutableArray array];
  NSMutableArray<id<ReadingListListItem>>* unreadArray = [NSMutableArray array];
  [self fillReadItems:readArray unreadItems:unreadArray];

  return [self currentSection:[self.dataSink readItems]
             isDifferentOfArray:readArray] ||
         [self currentSection:[self.dataSink unreadItems]
             isDifferentOfArray:unreadArray];
}

// Returns whether there is a difference between the elements contained in the
// |sectionIdentifier| and those in the |array|. The comparison is done with the
// URL of the elements. If an element exist in both, the one in |currentSection|
// will be overwriten with the informations contained in the one from|array|.
- (BOOL)currentSection:(NSArray<id<ReadingListListItem>>*)currentSection
    isDifferentOfArray:(NSArray<id<ReadingListListItem>>*)array {
  if (currentSection.count != array.count)
    return YES;

  NSMutableArray<id<ReadingListListItem>>* itemsToReconfigure =
      [NSMutableArray array];

  NSInteger index = 0;
  for (id<ReadingListListItem> newItem in array) {
    id<ReadingListListItem> oldItem = currentSection[index];
    if (oldItem.entryURL == newItem.entryURL) {
      if (![oldItem isEqual:newItem]) {
        [itemsToReconfigure addObject:oldItem];
        oldItem.title = newItem.title;
        oldItem.entryURL = newItem.entryURL;
        oldItem.distillationState = newItem.distillationState;
        oldItem.distillationDateText = newItem.distillationDateText;
        oldItem.distillationSizeText = newItem.distillationSizeText;
      }
      if (oldItem.faviconPageURL != newItem.faviconPageURL) {
        oldItem.faviconPageURL = newItem.faviconPageURL;
        [self fetchFaviconForItem:oldItem];
      }
    }
    if (![oldItem isEqual:newItem]) {
      return YES;
    }
    index++;
  }
  [self.dataSink itemsHaveChanged:itemsToReconfigure];
  return NO;
}

// Logs the deletions histograms for the entry associated with |item|.
- (void)logDeletionOfItem:(id<ReadingListListItem>)item {
  const ReadingListEntry* entry = [self entryFromItem:item];

  if (!entry)
    return;

  int64_t firstRead = entry->FirstReadTime();
  if (firstRead > 0) {
    // Log 0 if the entry has never been read.
    firstRead = (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds() -
                firstRead;
    // Convert it to hours.
    firstRead = firstRead / base::Time::kMicrosecondsPerHour;
  }
  UMA_HISTOGRAM_COUNTS_10000("ReadingList.FirstReadAgeOnDeletion", firstRead);

  int64_t age = (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds() -
                entry->CreationTime();
  // Convert it to hours.
  age = age / base::Time::kMicrosecondsPerHour;
  if (entry->IsRead())
    UMA_HISTOGRAM_COUNTS_10000("ReadingList.Read.AgeOnDeletion", age);
  else
    UMA_HISTOGRAM_COUNTS_10000("ReadingList.Unread.AgeOnDeletion", age);
}

@end
