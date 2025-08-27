// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_list_mediator.h"

#import <UIKit/UIKit.h>

#import <algorithm>
#import <memory>
#import <string>
#import <vector>

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_observer_bridge.h"
#import "ios/chrome/browser/download/model/download_record_service.h"
#import "ios/chrome/browser/download/ui/download_list_consumer.h"
#import "ios/web/public/download/download_task.h"

@interface DownloadListMediator () <DownloadRecordObserverDelegate> {
  raw_ptr<DownloadRecordService> _downloadRecordService;
  __weak id<DownloadListConsumer> _consumer;
  BOOL _isReady;
  std::unique_ptr<DownloadRecordObserverBridge> _observerBridge;
  // Flag to indicate if the app is recovering from background state.
  BOOL _isRecoveringFromBackground;
  // Current filter type applied to the download records.
  DownloadFilterType _currentFilterType;
  // Cached download records to avoid frequent service calls.
  std::vector<DownloadRecord> _allRecords;
}
@end

@implementation DownloadListMediator

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

  [_consumer setLoadingState:YES];

  std::vector<DownloadRecord> recordsToDisplay =
      [self applyCurrentFilter:_allRecords];

  [_consumer setDownloadRecords:recordsToDisplay];
  [_consumer setLoadingState:NO];
  [_consumer setEmptyState:(recordsToDisplay.size() == 0)];
}

- (void)syncRecordsIfNeeded {
  CHECK(_isReady);

  // Implement logic to sync records with the file system in a future iteration.

  std::vector<DownloadRecord> recordsToDisplay =
      [self applyCurrentFilter:_allRecords];
  [_consumer setDownloadRecords:recordsToDisplay];
}

- (void)filterRecordsWithType:(DownloadFilterType)type {
  CHECK(_isReady);

  _currentFilterType = type;

  [_consumer setLoadingState:YES];

  std::vector<DownloadRecord> filteredRecords =
      [self applyCurrentFilter:_allRecords];

  [_consumer setDownloadRecords:filteredRecords];
  [_consumer setLoadingState:NO];
  [_consumer setEmptyState:(filteredRecords.size() == 0)];
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

@end
