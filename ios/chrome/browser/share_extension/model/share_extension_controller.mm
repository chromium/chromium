// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/share_extension_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "ios/chrome/browser/share_extension/model/parsed_share_extension_entry.h"
#import "ios/chrome/browser/share_extension/model/share_extension_utils.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Enum used to send metrics on item reception.
// If you change this enum, update histograms.xml.
enum ShareExtensionSource {
  UNKNOWN_SOURCE = 0,
  SHARE_EXTENSION,
  SHARE_EXTENSION_SOURCE_COUNT
};

enum ShareExtensionItemReceived {
  INVALID_ENTRY = 0,
  CANCELLED_ENTRY,
  READINGLIST_ENTRY,
  BOOKMARK_ENTRY,
  OPEN_IN_CHROME_ENTRY,
  OPEN_IN_CHROME_INCOGNITO_ENTRY,
  IMAGE_SEARCH_ENTRY,
  TEXT_SEARCH_ENTRY,
  INCOGNITO_IMAGE_SEARCH_ENTRY,
  INCOGNITO_TEXT_SEARCH_ENTRY,
  SHARE_EXTENSION_ITEM_RECEIVED_COUNT
};

void DeleteFileAtUrl(NSURL* url) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  [[NSFileManager defaultManager] removeItemAtURL:url error:nil];
}

void LogHistogramReceivedItem(ShareExtensionItemReceived type) {
  UMA_HISTOGRAM_ENUMERATION("IOS.ShareExtension.ReceivedEntry", type,
                            SHARE_EXTENSION_ITEM_RECEIVED_COUNT);
}

ShareExtensionSource SourceIDFromSource(NSString* source) {
  if ([source isEqualToString:app_group::kShareItemSourceShareExtension]) {
    return SHARE_EXTENSION;
  }
  return UNKNOWN_SOURCE;
}

}  // namespace

@interface ShareExtensionController () <NSFilePresenter>
@end

@implementation ShareExtensionController {
  BOOL _isObservingReadingListFolder;
  BOOL _readingListFolderCreated;
  BOOL _shutdownCalled;
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
  SEQUENCE_CHECKER(_sequenceChecker);
}

#pragma mark - NSObject lifetime

- (void)dealloc {
  DCHECK(!_taskRunner) << "-shutdown must be called before -dealloc";
}

#pragma mark - Public

- (instancetype)init {
  self = [super init];

  if (![self presentedItemURL]) {
    return nil;
  }

  if (self) {
    _taskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  }

  return self;
}

- (void)shutdown {
  _shutdownCalled = YES;
  if (_isObservingReadingListFolder) {
    [NSFileCoordinator removeFilePresenter:self];
  }
  _taskRunner = nullptr;
}

- (void)startFilesProcessing {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_shutdownCalled) {
    return;
  }

  NSURL* filesFolderURL = [self presentedItemURL];

  if (!filesFolderURL) {
    return;
  }

  __weak ShareExtensionController* weakSelf = self;
  _taskRunner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateShareExtensionFilesDirectory, filesFolderURL),
      base::BindOnce(^(bool success) {
        [weakSelf handleReadingListFolderCreationWithSuccess:success];
      }));
}

#pragma mark - NSFilePresenter methods

- (void)presentedSubitemDidChangeAtURL:(NSURL*)url {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_shutdownCalled) {
    return;
  }
  [self handleFileAtURL:url];
}

- (NSOperationQueue*)presentedItemOperationQueue {
  return [NSOperationQueue mainQueue];
}

- (NSURL*)presentedItemURL {
  return app_group::ExternalCommandsItemsFolder();
}

#pragma mark - Private

- (void)startObservingReadingListFolder {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK(!_shutdownCalled);
  if (!_isObservingReadingListFolder) {
    _isObservingReadingListFolder = YES;
    [NSFileCoordinator addFilePresenter:self];
    [self processExistingFiles];
  }
}

- (void)stopObservingReadingListFolder {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isObservingReadingListFolder) {
    _isObservingReadingListFolder = NO;
    [NSFileCoordinator removeFilePresenter:self];
  }
}

- (void)handleReadingListFolderCreationWithSuccess:(BOOL)success {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_shutdownCalled) {
    return;
  }

  if (success) {
    [self readingListFolderCreated];
  }
}

- (void)readingListFolderCreated {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_shutdownCalled) {
    return;
  }
  _readingListFolderCreated = YES;

  [self startObservingReadingListFolder];
}

- (void)applicationDidBecomeActive {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK(!_shutdownCalled);
  if (!_readingListFolderCreated) {
    return;
  }

  [self startObservingReadingListFolder];
  // There may already be files. Process them.
  [self processExistingFiles];
}

- (void)applicationWillResignActive {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK(!_shutdownCalled);
  [self stopObservingReadingListFolder];
}

- (void)processExistingFiles {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_shutdownCalled) {
    return;
  }

  NSURL* filesDirectory = [self presentedItemURL];

  if (!filesDirectory) {
    return;
  }

  __weak ShareExtensionController* weakSelf = self;

  _taskRunner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&EnumerateFilesInDirectory, filesDirectory),
      base::BindOnce(^(NSArray<NSURL*>* files) {
        [weakSelf handleDirectoryEnumerationResult:files];
      }));
}

- (void)handleDirectoryEnumerationResult:(NSArray<NSURL*>*)files {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  if (_shutdownCalled) {
    return;
  }

  const NSUInteger filesCount = [files count];
  if (filesCount) {
    UMA_HISTOGRAM_COUNTS_100("IOS.ShareExtension.ReceivedEntriesCount",
                             filesCount);

    for (NSURL* fileURL : files) {
      [self handleFileAtURL:fileURL];
    }
  }
}

- (void)handleFileAtURL:(NSURL*)url {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK(_shutdownCalled);

  __weak ShareExtensionController* weakSelf = self;

  ProceduralBlock successCompletion = base::CallbackToBlock(
      base::BindPostTask(_taskRunner, base::BindOnce(&DeleteFileAtUrl, url)));

  _taskRunner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&PerformBlockingFileReadAndParse, url),
      base::BindOnce(^(ParsedShareExtensionEntry* parsedEntry) {
        [weakSelf handleParsedShareEntryResult:parsedEntry
                               originalFileURL:url
                                    completion:successCompletion];
      }));
}

- (void)handleParsedShareEntryResult:(ParsedShareExtensionEntry*)parsedEntry
                     originalFileURL:(NSURL*)originalFileURL
                          completion:(ProceduralBlock)completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_shutdownCalled) {
    return;
  }

  if (![parsedEntry parsedEntryIsValid]) {
    LogHistogramReceivedItem(INVALID_ENTRY);
    if (completion) {
      completion();
    }
    return;
  }

  if (parsedEntry.cancelled) {
    LogHistogramReceivedItem(CANCELLED_ENTRY);
    if (completion) {
      completion();
    }
    return;
  }

  UMA_HISTOGRAM_TIMES(
      "IOS.ShareExtension.ReceivedEntryDelay",
      base::Seconds([[NSDate date] timeIntervalSinceDate:parsedEntry.date]));
  UMA_HISTOGRAM_ENUMERATION("IOS.ShareExtension.Source",
                            SourceIDFromSource(parsedEntry.source),
                            SHARE_EXTENSION_SOURCE_COUNT);

  [self processEntryWithType:parsedEntry.type
                       title:parsedEntry.title
                         URL:parsedEntry.url
                  completion:completion];
}

- (void)processEntryWithType:(app_group::ShareExtensionItemType)entryType
                       title:(NSString*)entryTitle
                         URL:(NSURL*)entryURL
                  completion:(ProceduralBlock)completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // TODO(crbug.com/40260909): Bookmark the URL or add it to reading list.

  if (completion) {
    completion();
  }
}

@end
