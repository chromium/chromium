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
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/i18n/string_search.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_observer_bridge.h"
#import "ios/chrome/browser/download/model/download_record_service.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_consumer.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"
#import "ios/web/public/download/download_task.h"

using base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents;

// Type alias for file existence categorization result.
using CategorizationResult =
    std::pair<std::vector<DownloadRecord>, std::vector<DownloadRecord>>;

@interface DownloadListMediator () <DownloadRecordObserverDelegate>

// Cached download records to avoid frequent service calls.
@property(nonatomic, assign) std::vector<DownloadRecord> allRecords;

// Cached filtered records to optimize incremental search.
@property(nonatomic, assign) std::vector<DownloadRecord> filteredRecordsCache;

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

  // Current search keyword applied to the download records.
  NSString* _currentSearchKeyword;

  // Flag indicating if this is an incognito session.
  BOOL _isIncognito;
}

- (instancetype)initWithDownloadRecordService:
                    (DownloadRecordService*)downloadRecordService
                                  isIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    CHECK(downloadRecordService);
    _downloadRecordService = downloadRecordService;
    _isIncognito = isIncognito;
    _observerBridge = std::make_unique<DownloadRecordObserverBridge>(self);
    _currentFilterType = DownloadFilterType::kAll;
    _currentSearchKeyword = @"";
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
  [self loadDownloadRecordsWithLoading:YES checkFileExistence:YES];
}

- (void)syncRecordsIfNeeded {
  CHECK(_isReady);

  // Check file existence for all cached records and remove missing ones.
  if (self.allRecords.empty()) {
    return;
  }

  // Directly check file existence since allRecords is already filtered.
  __weak __typeof__(self) weakSelf = self;
  [self categorizeDownloadRecords:self.allRecords
                completionHandler:^(std::vector<DownloadRecord> existingFiles,
                                    std::vector<DownloadRecord> missingFiles) {
                  __strong __typeof__(weakSelf) strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }

                  // Update UI with existing files and clean up missing ones.
                  [strongSelf handleValidatedRecords:std::move(existingFiles)
                                      invalidRecords:std::move(missingFiles)
                                         showLoading:NO];
                }];
}

- (void)filterRecordsWithType:(DownloadFilterType)type {
  CHECK(_isReady);

  _currentFilterType = type;

  // Clear search cache when filter type changes.
  [self invalidateSearchCache];

  std::vector<DownloadRecord> filteredRecords =
      [self applyFilterWithTypeAndKeyword:self.allRecords];

  // Update cache.
  self.filteredRecordsCache = filteredRecords;

  [self setDownloadListItems:filteredRecords];
}

- (void)filterRecordsWithKeyword:(NSString*)keyword {
  CHECK(_isReady);

  NSString* normalizedKeyword = [self normalizeKeyword:keyword];

  // Avoid unnecessary re-filtering if keyword hasn't actually changed.
  if ([_currentSearchKeyword isEqualToString:normalizedKeyword]) {
    return;
  }

  NSString* previousKeyword = _currentSearchKeyword;
  _currentSearchKeyword = normalizedKeyword;

  std::vector<DownloadRecord> filteredRecords;

  // Incremental search optimization: if new keyword extends previous keyword,
  // filter on cached results.
  if ([self keyword:normalizedKeyword extendsKeyword:previousKeyword]) {
    filteredRecords =
        [self applyKeywordFilterToRecords:self.filteredRecordsCache
                                  keyword:normalizedKeyword];
  } else {
    // Otherwise, filter from all records and update cache.
    filteredRecords = [self applyFilterWithTypeAndKeyword:self.allRecords];
  }

  // Update cache.
  self.filteredRecordsCache = filteredRecords;

  [self setDownloadListItems:filteredRecords];
}

- (void)deleteDownloadItem:(DownloadListItem*)item {
  NSString* downloadID = item.downloadID;
  std::string downloadIdString = base::SysNSStringToUTF8(downloadID);
  base::FilePath filePath = item.filePath;

  // Delete file from disk first, then remove the record if deletion succeeds.
  __weak __typeof__(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::DeleteFile, filePath),
      base::BindOnce(^(bool success) {
        [weakSelf handleFileDeletionResult:success
                             forDownloadID:downloadIdString];
      }));
}

- (void)handleFileDeletionResult:(bool)success
                   forDownloadID:(const std::string&)downloadIdString {
  if (!_downloadRecordService) {
    return;
  }
  // Canceled or failed downloads may not have files to delete.
  // Just remove the record regardless of file deletion result.
  _downloadRecordService->RemoveDownloadByIdAsync(downloadIdString);
}

- (void)cancelDownloadItem:(DownloadListItem*)item {
  web::DownloadTask* downloadTask = _downloadRecordService->GetDownloadTaskById(
      base::SysNSStringToUTF8(item.downloadID));
  if (!downloadTask) {
    return;
  }
  downloadTask->Cancel();
}

#pragma mark - DownloadRecordObserver Methods

- (void)downloadRecordWasAdded:(const DownloadRecord&)record {
  // Only update if record matches current incognito state.
  if (!_isIncognito && record.is_incognito) {
    return;
  }

  [self invalidateSearchCache];
  [self updateConsumer];
}

- (void)downloadRecordWasUpdated:(const DownloadRecord&)record {
  // Only update if record matches current incognito state.
  if (!_isIncognito && record.is_incognito) {
    return;
  }

  // Only update if the record exists in our cached allRecords.
  NSString* recordID = base::SysUTF8ToNSString(record.download_id);
  if (![self containsRecordsWithIDs:@[ recordID ]]) {
    return;
  }

  [self invalidateSearchCache];
  [self updateConsumer];
}

- (void)downloadsWereRemovedWithIDs:(NSArray<NSString*>*)downloadIDs {
  // Check if any of the removed IDs exist in our cached allRecords.
  if (![self containsRecordsWithIDs:downloadIDs]) {
    return;
  }

  // Found matching records, update UI.
  [self invalidateSearchCache];
  [self updateConsumer];
}

#pragma mark - Private Methods

// Loads download records with optional loading indicator and records
// categorization (checking file existence only for completed downloads).
- (void)loadDownloadRecordsWithLoading:(BOOL)showLoading
                    checkFileExistence:(BOOL)checkFileExistence {
  CHECK(_isReady);

  if (showLoading) {
    [self.consumer setLoadingState:YES];
  }

  // Clear cache since we're reloading all data.
  [self invalidateSearchCache];

  __weak __typeof__(self) weakSelf = self;
  _downloadRecordService->GetAllDownloadsAsync(
      base::BindOnce(^(std::vector<DownloadRecord> records) {
        __strong __typeof__(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf handleDownloadRecordsResult:std::move(records)
                                    showLoading:showLoading
                             checkFileExistence:checkFileExistence];
      }));
}

// Handles the result from GetAllDownloadsAsync by filtering and updating the
// UI.
- (void)handleDownloadRecordsResult:(std::vector<DownloadRecord>)records
                        showLoading:(BOOL)showLoading
                 checkFileExistence:(BOOL)checkFileExistence {
  // Filter incognito records at the data source level.
  // If current session is not incognito, filter out incognito records.
  std::vector<DownloadRecord> filteredByIncognito;
  for (const auto& record : records) {
    if (_isIncognito || !record.is_incognito) {
      filteredByIncognito.push_back(record);
    }
  }

  if (checkFileExistence) {
    // Categorize records (check file existence only for completed downloads).
    __weak __typeof__(self) weakSelf = self;
    [self
        categorizeDownloadRecords:filteredByIncognito
                completionHandler:^(std::vector<DownloadRecord> existingFiles,
                                    std::vector<DownloadRecord> missingFiles) {
                  __strong __typeof__(weakSelf) strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  [strongSelf handleValidatedRecords:std::move(existingFiles)
                                      invalidRecords:std::move(missingFiles)
                                         showLoading:showLoading];
                }];
  } else {
    // Skip record categorization.
    [self updateRecordsAndUI:std::move(filteredByIncognito)
                 showLoading:showLoading];
  }
}

// Handles validated records by updating UI and cleaning up invalid ones.
- (void)handleValidatedRecords:(std::vector<DownloadRecord>)validRecords
                invalidRecords:(std::vector<DownloadRecord>)invalidRecords
                   showLoading:(BOOL)showLoading {
  // Update UI with valid records only.
  [self updateRecordsAndUI:std::move(validRecords) showLoading:showLoading];

  // Remove invalid records from service in background.
  [self removeDownloadRecords:invalidRecords];
}

// Updates records and UI with the given records.
- (void)updateRecordsAndUI:(std::vector<DownloadRecord>)records
               showLoading:(BOOL)showLoading {
  self.allRecords = std::move(records);
  std::vector<DownloadRecord> recordsToDisplay =
      [self applyFilterWithTypeAndKeyword:self.allRecords];

  // Update cache.
  self.filteredRecordsCache = recordsToDisplay;
  if (showLoading) {
    [self.consumer setLoadingState:NO];
  }
  [self setDownloadListItems:recordsToDisplay];
}

// Applies the current filter type and keyword to the given records.
- (std::vector<DownloadRecord>)applyFilterWithTypeAndKeyword:
    (const std::vector<DownloadRecord>&)records {
  std::vector<DownloadRecord> filteredRecords;

  for (const auto& record : records) {
    // Apply filter type check.
    BOOL matchesFilter =
        (_currentFilterType == DownloadFilterType::kAll) ||
        IsDownloadFilterMatch(record.mime_type, _currentFilterType);

    // Apply keyword search check.
    BOOL matchesSearch = [self record:record
                       matchesKeyword:_currentSearchKeyword];

    if (matchesFilter && matchesSearch) {
      filteredRecords.push_back(record);
    }
  }

  return filteredRecords;
}

// Updates the consumer with the current download records.
- (void)updateConsumer {
  [self loadDownloadRecordsWithLoading:NO checkFileExistence:NO];
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

// Sets the download list items in the consumer.
- (void)setDownloadListItems:(const std::vector<DownloadRecord>&)records {
  NSMutableArray<DownloadListItem*>* items = [NSMutableArray array];
  for (const auto& record : records) {
    DownloadListItem* item =
        [[DownloadListItem alloc] initWithDownloadRecord:record];
    [items addObject:item];
  }
  [self.consumer setDownloadListItems:items.copy];
  [self.consumer setEmptyState:(items.count == 0)];

  // Hide filter view when there are no records at all.
  // Check if allRecords is empty, which means no records exist.
  BOOL hasAnyRecords = !self.allRecords.empty();
  [self.consumer setDownloadListHeaderShown:hasAnyRecords];
}

// Normalizes the search keyword by trimming whitespace and collapsing multiple
// spaces.
- (NSString*)normalizeKeyword:(NSString*)keyword {
  if (!keyword) {
    return @"";
  }

  // Trim leading and trailing whitespace and newlines.
  NSString* trimmedKeyword = [keyword
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];

  // Collapse multiple consecutive whitespace characters into single spaces.
  NSRegularExpression* regex =
      [NSRegularExpression regularExpressionWithPattern:@"\\s+"
                                                options:0
                                                  error:nil];
  NSString* normalizedKeyword = [regex
      stringByReplacingMatchesInString:trimmedKeyword
                               options:0
                                 range:NSMakeRange(0, trimmedKeyword.length)
                          withTemplate:@" "];

  return normalizedKeyword;
}

// Checks if a download record matches the given search keyword.
- (BOOL)record:(const DownloadRecord&)record matchesKeyword:(NSString*)keyword {
  // If no keyword is provided, match all records.
  if (!keyword || keyword.length == 0) {
    return YES;
  }

  // Create searcher for the current keyword.
  std::u16string keywordU16 = base::SysNSStringToUTF16(keyword);
  FixedPatternStringSearchIgnoringCaseAndAccents query_search(keywordU16);

  // Search in file name.
  std::u16string fileName = base::UTF8ToUTF16(record.file_name);
  if (query_search.Search(fileName, /*match_index=*/nullptr,
                          /*match_length=*/nullptr)) {
    return YES;
  }

  // Search in original URL.
  std::u16string originalUrl = base::UTF8ToUTF16(record.original_url);
  if (query_search.Search(originalUrl, /*match_index=*/nullptr,
                          /*match_length=*/nullptr)) {
    return YES;
  }

  return NO;
}

// Determines if the current search is incremental (new keyword extends previous
// keyword).
- (BOOL)keyword:(NSString*)newKeyword
    extendsKeyword:(NSString*)previousKeyword {
  // If there was no previous keyword, this is not an incremental search.
  if (!previousKeyword || previousKeyword.length == 0) {
    return NO;
  }

  // If the new keyword is empty, this is not an incremental search.
  if (!newKeyword || newKeyword.length == 0) {
    return NO;
  }

  // If the new keyword is shorter than the previous one, this is not
  // incremental.
  if (newKeyword.length <= previousKeyword.length) {
    return NO;
  }

  // Check if the new keyword starts with the previous keyword (incremental
  // search).
  return [newKeyword hasPrefix:previousKeyword];
}

// Applies keyword filter to the given records (used for incremental search).
- (std::vector<DownloadRecord>)applyKeywordFilterToRecords:
                                   (const std::vector<DownloadRecord>&)records
                                                   keyword:(NSString*)keyword {
  std::vector<DownloadRecord> filteredRecords;

  for (const auto& record : records) {
    if ([self record:record matchesKeyword:keyword]) {
      filteredRecords.push_back(record);
    }
  }

  return filteredRecords;
}

// Checks if any of the provided download IDs exist in cached allRecords.
- (BOOL)containsRecordsWithIDs:(NSArray<NSString*>*)downloadIDs {
  // Create NSSet from downloadIDs for O(1) lookup time
  NSSet<NSString*>* downloadIDSet = [NSSet setWithArray:downloadIDs];

  // Loop through self.allRecords and check for set membership
  for (const auto& record : self.allRecords) {
    NSString* recordID = base::SysUTF8ToNSString(record.download_id);
    if ([downloadIDSet containsObject:recordID]) {
      return YES;
    }
  }
  return NO;
}

// Invalidates the search cache when filter type changes.
- (void)invalidateSearchCache {
  self.filteredRecordsCache.clear();
}

// Categorizes records by file existence.
- (CategorizationResult)categorizeRecordsByFileExistence:
    (std::vector<DownloadRecord>)records {
  std::vector<DownloadRecord> existingFiles;
  std::vector<DownloadRecord> missingFiles;

  for (const auto& record : records) {
    // Only check file existence for Complete downloads, as non-Complete
    // downloads should always be considered "existing" regardless of file
    // state.
    if (record.state == web::DownloadTask::State::kComplete) {
      // Convert relative path to absolute path and check if file exists.
      bool fileExists = false;
      if (!record.file_path.empty()) {
        base::FilePath absolutePath =
            ConvertToAbsoluteDownloadPath(record.file_path);
        fileExists = base::PathExists(absolutePath);
      }

      // Complete downloads without files should be filtered out.
      if (!fileExists) {
        missingFiles.push_back(record);
      } else {
        existingFiles.push_back(record);
      }
    } else {
      // Non-Complete downloads (InProgress, Failed, Cancelled, etc.) are always
      // considered "existing" since they don't require file existence
      // validation.
      existingFiles.push_back(record);
    }
  }

  return std::make_pair(std::move(existingFiles), std::move(missingFiles));
}

// Categorizes download records asynchronously, checking file existence only
// for completed downloads and returns categorized results.
- (void)categorizeDownloadRecords:(const std::vector<DownloadRecord>&)records
                completionHandler:
                    (void (^)(std::vector<DownloadRecord> existingFiles,
                              std::vector<DownloadRecord> missingFiles))
                        completionHandler {
  __weak __typeof__(self) weakSelf = self;

  // The task to run on a background thread.
  auto backgroundTask = base::BindOnce(
      ^(std::vector<DownloadRecord> recordsToCheck) {
        return [weakSelf categorizeRecordsByFileExistence:recordsToCheck];
      },
      std::move(records));

  // The reply to run on the original sequence upon task completion.
  auto replyCallback = base::BindOnce(^(CategorizationResult result) {
    __strong __typeof__(weakSelf) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    auto [recordsWithFiles, recordsWithoutFiles] = std::move(result);
    completionHandler(std::move(recordsWithFiles),
                      std::move(recordsWithoutFiles));
  });

  // Post the task and its reply.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      std::move(backgroundTask), std::move(replyCallback));
}

// Removes the given records using the download record service.
- (void)removeDownloadRecords:(const std::vector<DownloadRecord>&)records {
  if (!_downloadRecordService) {
    return;
  }

  for (const auto& record : records) {
    _downloadRecordService->RemoveDownloadByIdAsync(record.download_id);
  }
}

@end
