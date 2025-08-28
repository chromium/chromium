// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_list_mediator.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <string>
#import <vector>

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_observer_bridge.h"
#import "ios/chrome/browser/download/model/download_record_service.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_consumer.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"
#import "ios/web/public/download/download_task.h"

@interface DownloadListMediator () <DownloadRecordObserverDelegate>

// Cached download records to avoid frequent service calls.
@property(nonatomic, assign) std::vector<DownloadRecord> allRecords;

// Consumer for updating the UI.
@property(nonatomic, weak) id<DownloadListConsumer> consumer;

@end

@implementation DownloadListMediator {
  // Service for managing download records.
  raw_ptr<DownloadRecordService> _downloadRecordService;

  // Flag to indicate if the mediator is ready (i.e., has a consumer).
  BOOL _isReady;

  // Observer bridge for download record updates.
  std::unique_ptr<DownloadRecordObserverBridge> _observerBridge;

  // Flag to indicate if the app is recovering from background state.
  BOOL _isRecoveringFromBackground;

  // Current filter type applied to the download records.
  DownloadFilterType _currentFilterType;
}

- (instancetype)initWithDownloadRecordService:
    (DownloadRecordService*)downloadRecordService {
  self = [super init];
  if (self) {
    CHECK(downloadRecordService);
    _downloadRecordService = downloadRecordService;
    _observerBridge = std::make_unique<DownloadRecordObserverBridge>(self);
    _currentFilterType = DownloadFilterType::kAll;
  }
  return self;
}

#pragma mark - Public

- (void)setConsumer:(id<DownloadListConsumer>)consumer {
  _consumer = consumer;
  _isReady = consumer != nil;
}

- (void)connect {
  _downloadRecordService->AddObserver(_observerBridge.get());
  [self setupApplicationObservers];
}

- (void)disconnect {
  [self removeApplicationObservers];
  if (_downloadRecordService && _observerBridge) {
    _downloadRecordService->RemoveObserver(_observerBridge.get());
  }
  _observerBridge.reset();
  _downloadRecordService = nullptr;
  _isReady = NO;
}

#pragma mark - Observers

- (void)setupApplicationObservers {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleApplicationDidBecomeActive)
             name:UIApplicationDidBecomeActiveNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleApplicationWillResignActive)
             name:UIApplicationWillResignActiveNotification
           object:nil];
}

- (void)removeApplicationObservers {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:nil];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationWillResignActiveNotification
              object:nil];
}

#pragma mark - DownloadListMutator

- (void)loadDownloadRecords {
  CHECK(_isReady);

  [self.consumer setLoadingState:YES];

  __weak __typeof__(self) weakSelf = self;
  _downloadRecordService->GetAllDownloadsAsync(
      base::BindOnce(^(std::vector<DownloadRecord> records) {
        __strong __typeof__(weakSelf) strongSelf = weakSelf;
        strongSelf.allRecords = std::move(records);
        std::vector<DownloadRecord> recordsToDisplay =
            [strongSelf applyCurrentFilter:strongSelf.allRecords];

        [strongSelf setDownloadListItems:recordsToDisplay];
      }));
}

- (void)syncRecordsIfNeeded {
  CHECK(_isReady);

  // Implement logic to sync records with the file system in a future iteration.

  std::vector<DownloadRecord> recordsToDisplay =
      [self applyCurrentFilter:self.allRecords];
  [self setDownloadListItems:recordsToDisplay];
}

- (void)filterRecordsWithType:(DownloadFilterType)type {
  CHECK(_isReady);

  _currentFilterType = type;

  [self.consumer setLoadingState:YES];

  std::vector<DownloadRecord> filteredRecords =
      [self applyCurrentFilter:self.allRecords];

  [self setDownloadListItems:filteredRecords];
}

#pragma mark - DownloadRecordObserver Methods

- (void)downloadRecordWasAdded:(const DownloadRecord&)record {
  [self updateConsumer];
}

- (void)downloadRecordWasUpdated:(const DownloadRecord&)record {
  [self updateConsumer];
}

- (void)downloadsWereRemovedWithIDs:(NSArray<NSString*>*)downloadIDs {
  [self updateConsumer];
}

#pragma mark - Private Methods

// Applies the current filter to the given records.
- (std::vector<DownloadRecord>)applyCurrentFilter:
    (const std::vector<DownloadRecord>&)records {
  if (_currentFilterType == DownloadFilterType::kAll) {
    return records;
  }

  std::vector<DownloadRecord> filteredRecords;
  for (const auto& record : records) {
    if (IsDownloadFilterMatch(record.mime_type, _currentFilterType)) {
      filteredRecords.push_back(record);
    }
  }

  return filteredRecords;
}

// Updates the consumer with the current download records.
- (void)updateConsumer {
  [self loadDownloadRecords];
}

// Handles the application did become active notification.
- (void)handleApplicationDidBecomeActive {
  if (_isRecoveringFromBackground) {
    _isRecoveringFromBackground = NO;
    // Sync records if needed.
    [self syncRecordsIfNeeded];
  }
}

// Handles the application will resign active notification.
- (void)handleApplicationWillResignActive {
  // Set flag to indicate the app is resigning active state.
  _isRecoveringFromBackground = YES;
}

/// Sets the download list items in the consumer.
- (void)setDownloadListItems:(const std::vector<DownloadRecord>&)records {
  NSMutableArray<DownloadListItem*>* items = [NSMutableArray array];
  for (const auto& record : records) {
    DownloadListItem* item =
        [[DownloadListItem alloc] initWithDownloadRecord:record];
    [items addObject:item];
  }
  [self.consumer setDownloadListItems:items.copy];
  [self.consumer setLoadingState:NO];
  [self.consumer setEmptyState:(items.count == 0)];
}

@end
