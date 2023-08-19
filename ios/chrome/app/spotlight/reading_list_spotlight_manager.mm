// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/reading_list_spotlight_manager.h"

#import <CoreSpotlight/CoreSpotlight.h>
#import <memory>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/elapsed_timer.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#import "ios/chrome/app/spotlight/reading_list_spotlight_manager.mm"
#import "ios/chrome/app/spotlight/searchable_item_factory.h"
#import "ios/chrome/app/spotlight/spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"

// Called from the BrowserBookmarkModelBridge from C++ -> ObjC.
@interface ReadingListSpotlightManager () <ReadingListModelBridgeObserver> {
  // Bridge to register for reading list changes.
  std::unique_ptr<ReadingListModelBridge> _modelBridge;

  /// Used when the model is in batch update mode.
  /// Keys are URLs of reading list items to update.
  /// Values are: true if the item needs to be added; false if it needs to be
  /// deleted. Reset when batch updates are done.
  std::map<GURL, bool> _batch_update_log;
}

/// Tracks reentrant batch updates of the model. A value of 0 indicates that the
/// model is not in batch updates mode and vice versa.
@property(nonatomic, assign) NSInteger modelUpdateDepth;

@end

@implementation ReadingListSpotlightManager

+ (ReadingListSpotlightManager*)readingListSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState {
  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(browserState);

  return [[ReadingListSpotlightManager alloc]
      initWithLargeIconService:largeIconService
              readingListModel:ReadingListModelFactory::GetInstance()
                                   ->GetForBrowserState(browserState)
            spotlightInterface:[SpotlightInterface defaultInterface]
         searchableItemFactory:
             [[SearchableItemFactory alloc]
                 initWithLargeIconService:largeIconService
                                   domain:spotlight::DOMAIN_READING_LIST
                    useTitleInIdentifiers:NO]];
}

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
            readingListModel:(ReadingListModel*)model
          spotlightInterface:(SpotlightInterface*)spotlightInterface
       searchableItemFactory:(SearchableItemFactory*)searchableItemFactory {
  self = [super init];

  if (self) {
    _model = model;
    _searchableItemFactory = searchableItemFactory;
    _spotlightInterface = spotlightInterface;
    _modelBridge.reset(new ReadingListModelBridge(self, model));
  }
  return self;
}

- (void)detachModel {
  _modelBridge.reset();
  _model = nil;
}

- (void)shutdown {
  [self detachModel];
}

- (void)clearAndReindexReadingList {
  if (!self.model || !self.model->loaded()) {
    [SpotlightLogger logSpotlightError:[ReadingListSpotlightManager
                                           modelNotReadyOrShutDownError]];
    return;
  }

  __weak ReadingListSpotlightManager* weakSelf = self;
  [self.searchableItemFactory cancelItemsGeneration];
  [self.spotlightInterface
      deleteSearchableItemsWithDomainIdentifiers:@[
        spotlight::StringFromSpotlightDomain(spotlight::DOMAIN_READING_LIST)
      ]
                               completionHandler:^(NSError* error) {
                                 if (error) {
                                   [SpotlightLogger logSpotlightError:error];
                                   return;
                                 }
                                 [weakSelf indexAllReadingListItems];
                               }];
}

- (void)indexAllReadingListItems {
  if (!self.model || !self.model->loaded()) {
    [SpotlightLogger logSpotlightError:[ReadingListSpotlightManager
                                           modelNotReadyOrShutDownError]];
    return;
  }

  const base::ElapsedTimer timer;

  for (const auto& url : self.model->GetKeys()) {
    scoped_refptr<const ReadingListEntry> entry =
        self.model->GetEntryByURL(url).get();
    DCHECK(entry);
    NSString* title = base::SysUTF8ToNSString(entry->Title());
    [self.searchableItemFactory
        generateSearchableItem:entry->URL()
                         title:title
            additionalKeywords:@[]
             completionHandler:^(CSSearchableItem* item) {
               [self.spotlightInterface indexSearchableItems:@[ item ]];
             }];
  }

  UMA_HISTOGRAM_TIMES("IOS.Spotlight.ReadingListIndexingDuration",
                      timer.Elapsed());
  UMA_HISTOGRAM_COUNTS_1000("IOS.Spotlight.ReadingListIndexSize",
                            self.model->size());
}

+ (NSError*)modelNotReadyOrShutDownError {
  return [NSError
      errorWithDomain:@"chrome"
                 code:0
             userInfo:@{
               NSLocalizedDescriptionKey :
                   @"Reading list model isn't initialized or already shut down"
             }];
}

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  [self clearAndReindexReadingList];
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
}

- (void)readingListModel:(const ReadingListModel*)model
         willRemoveEntry:(const GURL&)url {
  [self removeReadingListEntryFromSpotlight:url];
}

- (void)readingListModel:(const ReadingListModel*)model
             didAddEntry:(const GURL&)url
             entrySource:(reading_list::EntrySource)source {
  [self addReadingListEntryToSpotlight:url];
}

- (void)readingListModel:(const ReadingListModel*)model
         willUpdateEntry:(const GURL&)url {
  /// Since it's unknown what will be updated, remove the existing entry and
  /// re-add the updated one in `readingListModel:didUpdateEntry:` below
  [self removeReadingListEntryFromSpotlight:url];
}

- (void)readingListModel:(const ReadingListModel*)model
          didUpdateEntry:(const GURL&)url {
  /// See comment in `willUpdateEntry`.
  [self addReadingListEntryToSpotlight:url];
}

- (void)readingListModelBeganBatchUpdates:(const ReadingListModel*)model {
  self.modelUpdateDepth += 1;
}

- (void)readingListModelCompletedBatchUpdates:(const ReadingListModel*)model {
  self.modelUpdateDepth -= 1;
  if (self.modelUpdateDepth != 0) {
    return;
  }

  // Apply batch updates:
  // For entries to add, just add them. No need to batch this, because they need
  // to fetch a favicon, which is already queued and async. For entries to
  // delete, build a list to batch remove them from the index.
  NSMutableArray<NSString*>* entriesToRemove = [[NSMutableArray alloc] init];
  for (auto p : _batch_update_log) {
    if (p.second) {
      [self addReadingListEntryToSpotlight:p.first];
    } else {
      [entriesToRemove
          addObject:[self.searchableItemFactory spotlightIDForURL:p.first]];
    }
  }

  if (entriesToRemove.count > 0) {
    [self.spotlightInterface
        deleteSearchableItemsWithIdentifiers:entriesToRemove
                           completionHandler:nil];
  }

  _batch_update_log.clear();
}

#pragma mark - private

- (void)addReadingListEntryToSpotlight:(const GURL&)url {
  if (self.modelUpdateDepth > 0) {
    _batch_update_log[url] = true;
    return;
  }

  DCHECK(self.model);
  scoped_refptr<const ReadingListEntry> entry =
      self.model->GetEntryByURL(url).get();
  DCHECK(entry);
  NSString* title = base::SysUTF8ToNSString(entry->Title());
  [self.searchableItemFactory
      generateSearchableItem:entry->URL()
                       title:title
          additionalKeywords:@[]
           completionHandler:^(CSSearchableItem* item) {
             [self.spotlightInterface indexSearchableItems:@[ item ]];
           }];
}

- (void)removeReadingListEntryFromSpotlight:(const GURL&)url {
  DCHECK(self.model);
  if (self.modelUpdateDepth > 0) {
    if (_batch_update_log.count(url)) {
      // In the same batch operation, this URL was added, and now being deleted.
      // Remove it from the update log, effectively never adding it to the
      // index.
      _batch_update_log.erase(url);
    } else {
      _batch_update_log[url] = false;
    }
    return;
  }

  scoped_refptr<const ReadingListEntry> entry =
      self.model->GetEntryByURL(url).get();
  DCHECK(entry);
  NSString* spotlightID = [self.searchableItemFactory spotlightIDForURL:url];
  [self.spotlightInterface deleteSearchableItemsWithIdentifiers:@[ spotlightID ]
                                              completionHandler:nil];
}

@end
