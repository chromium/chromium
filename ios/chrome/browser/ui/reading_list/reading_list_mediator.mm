// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_mediator.h"

#import <algorithm>

#import "base/apple/foundation_util.h"
#import "base/location.h"
#import "base/memory/scoped_refptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/features/reading_list_switches.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_data_sink.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_util.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"

namespace {
// Sorter function that orders ReadingListEntries by their update time.
bool EntrySorter(scoped_refptr<const ReadingListEntry> rhs,
                 scoped_refptr<const ReadingListEntry> lhs) {
  return rhs->UpdateTime() > lhs->UpdateTime();
}
}  // namespace

@interface ReadingListMediator () <ReadingListModelBridgeObserver,
                                   SyncObserverModelBridge>

// The model passed on initialization.
@property(nonatomic, assign) ReadingListModel* model;

// Whether the consumer should be notified of model changes.
@property(nonatomic, assign) BOOL shouldMonitorModel;

// The ListItem factory passed on initialization.
@property(nonatomic, strong) ReadingListListItemFactory* itemFactory;

// Favicon Service used for UIRefresh Collections.
@property(nonatomic, assign, readonly) FaviconLoader* faviconLoader;

@end

@implementation ReadingListMediator {
  std::unique_ptr<ReadingListModelBridge> _modelBridge;
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate> _batchToken;
  // Observer to keep track of the syncing status.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
}

@synthesize dataSink = _dataSink;

#pragma mark - Public

- (instancetype)initWithModel:(ReadingListModel*)model
                  syncService:(nonnull syncer::SyncService*)syncService
                faviconLoader:(nonnull FaviconLoader*)faviconLoader
              listItemFactory:(ReadingListListItemFactory*)itemFactory {
  self = [super init];
  if (self) {
    _model = model;
    _itemFactory = itemFactory;
    _shouldMonitorModel = YES;
    _faviconLoader = faviconLoader;
    _syncObserver = std::make_unique<SyncObserverBridge>(self, syncService);

    // This triggers the callback method. Should be created last.
    _modelBridge.reset(new ReadingListModelBridge(self, model));
  }
  return self;
}

- (scoped_refptr<const ReadingListEntry>)entryFromItem:
    (id<ReadingListListItem>)item {
  return self.model->GetEntryByURL(item.entryURL);
}

- (void)markEntryRead:(const GURL&)URL {
  self.model->SetReadStatusIfExists(URL, true);
}

- (void)disconnect {
  _dataSink = nil;
  _model = nullptr;
  _itemFactory = nil;
  _faviconLoader = nullptr;
  _modelBridge.reset();
  _syncObserver.reset();
}

- (void)dealloc {
  DCHECK(!_model);
}

#pragma mark - ReadingListDataSource

- (BOOL)isItemRead:(id<ReadingListListItem>)item {
  scoped_refptr<const ReadingListEntry> readingListEntry =
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
  self.model->SetReadStatusIfExists(item.entryURL, read);
}

- (scoped_refptr<const ReadingListEntry>)entryWithURL:(const GURL&)URL {
  return self.model->GetEntryByURL(URL);
}

- (void)removeEntryFromItem:(id<ReadingListListItem>)item {
  [self logDeletionOfItem:item];
  self.model->RemoveEntryByURL(item.entryURL, FROM_HERE);
}

- (void)fillReadItems:(NSMutableArray<id<ReadingListListItem>>*)readArray
          unreadItems:(NSMutableArray<id<ReadingListListItem>>*)unreadArray {
  std::vector<scoped_refptr<const ReadingListEntry>> readEntries;
  std::vector<scoped_refptr<const ReadingListEntry>> unreadEntries;

  for (const auto& url : self.model->GetKeys()) {
    scoped_refptr<const ReadingListEntry> entry =
        self.model->GetEntryByURL(url);
    DCHECK(entry);
    if (entry->IsRead()) {
      readEntries.push_back(std::move(entry));
    } else {
      unreadEntries.push_back(std::move(entry));
    }
  }

  std::sort(readEntries.begin(), readEntries.end(), EntrySorter);
  std::sort(unreadEntries.begin(), unreadEntries.end(), EntrySorter);

  for (scoped_refptr<const ReadingListEntry> entry : readEntries) {
    bool needsExplicitUpload =
        self.model->NeedsExplicitUploadToSyncServer(entry->URL());
    ListItem<ReadingListListItem>* item =
        [self.itemFactory cellItemForReadingListEntry:entry.get()
                                  needsExplicitUpload:needsExplicitUpload];
    [readArray addObject:item];
  }

  for (scoped_refptr<const ReadingListEntry> entry : unreadEntries) {
    bool needsExplicitUpload =
        self.model->NeedsExplicitUploadToSyncServer(entry->URL());
    ListItem<ReadingListListItem>* item =
        [self.itemFactory cellItemForReadingListEntry:entry.get()
                                  needsExplicitUpload:needsExplicitUpload];
    [unreadArray addObject:item];
  }

  DCHECK(self.model->GetKeys().size() ==
         [readArray count] + [unreadArray count]);
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
      item.faviconPageURL, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
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

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  // If the sync state, especially the account storage state changes, the UI
  // including cloud icons on items needs to be updated.
  if ([self hasDataSourceChanged]) {
    [self.dataSink dataSourceChanged];
  }
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
// `sectionIdentifier` and those in the `array`. The comparison is done with the
// URL of the elements. If an element exist in both, the one in `currentSection`
// will be overwriten with the informations contained in the one from `array`.
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
        oldItem.showCloudSlashIcon = newItem.showCloudSlashIcon;
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

// Logs the deletions histograms for the entry associated with `item`.
- (void)logDeletionOfItem:(id<ReadingListListItem>)item {
  scoped_refptr<const ReadingListEntry> entry = [self entryFromItem:item];

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
