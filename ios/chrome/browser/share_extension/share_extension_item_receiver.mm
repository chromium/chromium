// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/share_extension_item_receiver.h"

#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "base/ios/block_types.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "ios/chrome/browser/system_flags.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Enum used to send metrics on item reception.
// If you change this enum, update histograms.xml.
enum ShareExtensionItemReceived {
  INVALID_ENTRY = 0,
  CANCELLED_ENTRY,
  READINGLIST_ENTRY,
  BOOKMARK_ENTRY,
  OPEN_IN_CHROME_ENTRY,
  SHARE_EXTENSION_ITEM_RECEIVED_COUNT
};

// Enum used to send metrics on item reception.
// If you change this enum, update histograms.xml.
enum ShareExtensionSource {
  UNKNOWN_SOURCE = 0,
  SHARE_EXTENSION,
  SHARE_EXTENSION_SOURCE_COUNT
};

ShareExtensionSource SourceIDFromSource(NSString* source) {
  if ([source isEqualToString:app_group::kShareItemSourceShareExtension]) {
    return SHARE_EXTENSION;
  }
  return UNKNOWN_SOURCE;
}

void LogHistogramReceivedItem(ShareExtensionItemReceived type) {
  UMA_HISTOGRAM_ENUMERATION("IOS.ShareExtension.ReceivedEntry", type,
                            SHARE_EXTENSION_ITEM_RECEIVED_COUNT);
}

}  // namespace

@interface ShareExtensionItemReceiver ()<NSFilePresenter> {
  BOOL _isObservingReadingListFolder;
  BOOL _readingListFolderCreated;
  ReadingListModel* _readingListModel;
  bookmarks::BookmarkModel* _bookmarkModel;
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
}

// Checks if the reading list folder is already created and if not, create it.
- (void)createReadingListFolder;

// Invoked on UI thread once the reading list folder has been created.
- (void)readingListFolderCreated;

// Processes the data sent by the share extension. Data should be a NSDictionary
// serialized by +|NSKeyedArchiver archivedDataWithRootObject:|.
// |completion| is called if |data| has been fully processed.
- (BOOL)receivedData:(NSData*)data withCompletion:(ProceduralBlock)completion;

// Reads the file pointed by |url| and calls |receivedData:| on the content.
// If the file is processed, delete it.
// |completion| is only called if the file handling is completed without error.
- (void)handleFileAtURL:(NSURL*)url withCompletion:(ProceduralBlock)completion;

// Deletes the file pointed by |url| then call |completion|.
- (void)deleteFileAtURL:(NSURL*)url withCompletion:(ProceduralBlock)completion;

// Called on UIApplicationDidBecomeActiveNotification notification.
- (void)applicationDidBecomeActive;

// Processes files that are already in the folder and starts observing the
// app_group::ShareExtensionItemsFolder() folder for new files.
- (void)processExistingFiles;

// Invoked with the list of pre-existing files in the folder to process them.
- (void)entriesReceived:(NSArray<NSURL*>*)files;

// Called on UIApplicationWillResignActiveNotification. Stops observing the
// app_group::ShareExtensionItemsFolder() folder for new files.
- (void)applicationWillResignActive;

// Called whenever a file is modified in app_group::ShareExtensionItemsFolder().
- (void)presentedSubitemDidChangeAtURL:(NSURL*)url;

@end

@implementation ShareExtensionItemReceiver

#pragma mark - NSObject lifetime

- (void)dealloc {
  DCHECK(!_taskRunner) << "-shutdown must be called before -dealloc";
}

#pragma mark - Public API

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                     readingListModel:(ReadingListModel*)readingListModel {
  DCHECK(bookmarkModel);
  DCHECK(readingListModel);

  self = [super init];
  if (![self presentedItemURL])
    return nil;

  if (self) {
    _readingListModel = readingListModel;
    _bookmarkModel = bookmarkModel;
    _taskRunner =
        base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                         base::TaskPriority::BEST_EFFORT});

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidBecomeActive)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationWillResignActive)
               name:UIApplicationWillResignActiveNotification
             object:nil];

    __weak ShareExtensionItemReceiver* weakSelf = self;
    _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                            [weakSelf createReadingListFolder];
                          }));
  }

  return self;
}

- (void)shutdown {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  if (_isObservingReadingListFolder) {
    [NSFileCoordinator removeFilePresenter:self];
  }
  _readingListModel = nil;
  _bookmarkModel = nil;
  _taskRunner = nullptr;
}

#pragma mark - Private API

- (void)createReadingListFolder {
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    NSFileManager* manager = [NSFileManager defaultManager];
    if (![manager fileExistsAtPath:[[self presentedItemURL] path]]) {
      [manager createDirectoryAtPath:[[self presentedItemURL] path]
          withIntermediateDirectories:NO
                           attributes:nil
                                error:nil];
    }
  }

  __weak ShareExtensionItemReceiver* weakSelf = self;
  base::PostTask(FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
                   [weakSelf readingListFolderCreated];
                 }));
}

- (void)readingListFolderCreated {
  UIApplication* application = [UIApplication sharedApplication];
  if ([application applicationState] == UIApplicationStateActive) {
    _readingListFolderCreated = YES;
    [self applicationDidBecomeActive];
  }
}

- (BOOL)receivedData:(NSData*)data withCompletion:(ProceduralBlock)completion {
  NSError* error = nil;
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
  if (!unarchiver || error) {
    DLOG(WARNING) << "Error creating share extension item unarchiver: "
                  << base::SysNSStringToUTF8([error description]);
    return NO;
  }

  unarchiver.requiresSecureCoding = NO;

  id entryID = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
  NSDictionary* entry = base::mac::ObjCCast<NSDictionary>(entryID);
  if (!entry) {
    if (completion) {
      completion();
    }
    return NO;
  }

  NSNumber* cancelled = base::mac::ObjCCast<NSNumber>(
      [entry objectForKey:app_group::kShareItemCancel]);
  if (!cancelled) {
    if (completion) {
      completion();
    }
    return NO;
  }
  if ([cancelled boolValue]) {
    LogHistogramReceivedItem(CANCELLED_ENTRY);
    if (completion) {
      completion();
    }
    return YES;
  }

  GURL entryURL =
      net::GURLWithNSURL([entry objectForKey:app_group::kShareItemURL]);
  std::string entryTitle =
      base::SysNSStringToUTF8([entry objectForKey:app_group::kShareItemTitle]);
  NSDate* entryDate = base::mac::ObjCCast<NSDate>(
      [entry objectForKey:app_group::kShareItemDate]);
  NSNumber* entryType = base::mac::ObjCCast<NSNumber>(
      [entry objectForKey:app_group::kShareItemType]);
  NSString* entrySource = base::mac::ObjCCast<NSString>(
      [entry objectForKey:app_group::kShareItemSource]);

  if (!entryURL.is_valid() || !entrySource || !entryDate || !entryType ||
      !entryURL.SchemeIsHTTPOrHTTPS()) {
    if (completion) {
      completion();
    }
    return NO;
  }

  UMA_HISTOGRAM_TIMES("IOS.ShareExtension.ReceivedEntryDelay",
                      base::TimeDelta::FromSecondsD(
                          [[NSDate date] timeIntervalSinceDate:entryDate]));

  UMA_HISTOGRAM_ENUMERATION("IOS.ShareExtension.Source",
                            SourceIDFromSource(entrySource),
                            SHARE_EXTENSION_SOURCE_COUNT);

  // Entry is valid. Add it to the reading list model.
  ProceduralBlock processEntryBlock = ^{
    if (!_readingListModel || !_bookmarkModel) {
      // Models may have been deleted after the file
      // processing started.
      return;
    }
    app_group::ShareExtensionItemType type =
        static_cast<app_group::ShareExtensionItemType>(
            [entryType integerValue]);
    switch (type) {
      case app_group::READING_LIST_ITEM: {
        LogHistogramReceivedItem(READINGLIST_ENTRY);
        _readingListModel->AddEntry(entryURL, entryTitle,
                                    reading_list::ADDED_VIA_EXTENSION);
        break;
      }
      case app_group::BOOKMARK_ITEM: {
        LogHistogramReceivedItem(BOOKMARK_ENTRY);
        _bookmarkModel->AddURL(_bookmarkModel->mobile_node(), 0,
                               base::UTF8ToUTF16(entryTitle), entryURL);
        break;
      }
      case app_group::OPEN_IN_CHROME_ITEM: {
        LogHistogramReceivedItem(OPEN_IN_CHROME_ENTRY);
        // Open URL command is sent directly by the extension. No processing is
        // needed here.
        break;
      }
    }

    if (completion && _taskRunner) {
      _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                              completion();
                            }));
    }
  };
  base::PostTask(FROM_HERE, {web::WebThread::UI},
                 base::BindOnce(processEntryBlock));
  return YES;
}

- (void)handleFileAtURL:(NSURL*)url withCompletion:(ProceduralBlock)completion {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  if (![[NSFileManager defaultManager] fileExistsAtPath:[url path]]) {
    // The handler is called on file modification, including deletion. Check
    // that the file exists before continuing.
    return;
  }
  __weak ShareExtensionItemReceiver* weakSelf = self;
  ProceduralBlock successCompletion = ^{
    [weakSelf deleteFileAtURL:url withCompletion:completion];
  };
  void (^readingAccessor)(NSURL*) = ^(NSURL* newURL) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    NSFileManager* manager = [NSFileManager defaultManager];
    NSData* data = [manager contentsAtPath:[newURL path]];
    if (![weakSelf receivedData:data withCompletion:successCompletion]) {
      LogHistogramReceivedItem(INVALID_ENTRY);
    }
  };
  NSError* error = nil;
  NSFileCoordinator* readingCoordinator =
      [[NSFileCoordinator alloc] initWithFilePresenter:self];
  [readingCoordinator
      coordinateReadingItemAtURL:url
                         options:NSFileCoordinatorReadingWithoutChanges
                           error:&error
                      byAccessor:readingAccessor];
}

- (void)deleteFileAtURL:(NSURL*)url withCompletion:(ProceduralBlock)completion {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  void (^deletingAccessor)(NSURL*) = ^(NSURL* newURL) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    NSFileManager* manager = [NSFileManager defaultManager];
    [manager removeItemAtURL:newURL error:nil];
  };
  NSError* error = nil;
  NSFileCoordinator* deletingCoordinator =
      [[NSFileCoordinator alloc] initWithFilePresenter:self];
  [deletingCoordinator
      coordinateWritingItemAtURL:url
                         options:NSFileCoordinatorWritingForDeleting
                           error:&error
                      byAccessor:deletingAccessor];
  if (completion) {
    completion();
  }
}

- (void)applicationDidBecomeActive {
  if (!_readingListFolderCreated || _isObservingReadingListFolder) {
    return;
  }
  _isObservingReadingListFolder = YES;

  // Start observing for new files.
  [NSFileCoordinator addFilePresenter:self];

  // There may already be files. Process them.
  if (_taskRunner) {
    __weak ShareExtensionItemReceiver* weakSelf = self;
    _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                            [weakSelf processExistingFiles];
                          }));
  }
}

- (void)processExistingFiles {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSMutableArray<NSURL*>* files = [NSMutableArray array];
  NSFileManager* manager = [NSFileManager defaultManager];
  NSArray<NSURL*>* oldFiles = [manager
        contentsOfDirectoryAtURL:app_group::LegacyShareExtensionItemsFolder()
      includingPropertiesForKeys:nil
                         options:NSDirectoryEnumerationSkipsHiddenFiles
                           error:nil];
  [files addObjectsFromArray:oldFiles];

  NSArray<NSURL*>* newFiles =
      [manager contentsOfDirectoryAtURL:[self presentedItemURL]
             includingPropertiesForKeys:nil
                                options:NSDirectoryEnumerationSkipsHiddenFiles
                                  error:nil];
  [files addObjectsFromArray:newFiles];

  if ([files count]) {
    __weak ShareExtensionItemReceiver* weakSelf = self;
    base::PostTask(FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
                     [weakSelf entriesReceived:files];
                   }));
  }
}

- (void)entriesReceived:(NSArray<NSURL*>*)files {
  UMA_HISTOGRAM_COUNTS_100("IOS.ShareExtension.ReceivedEntriesCount",
                           [files count]);
  if (!_taskRunner)
    return;

  __weak ShareExtensionItemReceiver* weakSelf = self;
  for (NSURL* fileURL : files) {
    __block std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
        batchToken(_readingListModel->BeginBatchUpdates());
    _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                            [weakSelf handleFileAtURL:fileURL
                                       withCompletion:^{
                                         base::PostTask(FROM_HERE,
                                                        {web::WebThread::UI},
                                                        base::BindOnce(^{
                                                          batchToken.reset();
                                                        }));
                                       }];
                          }));
  }
}

- (void)applicationWillResignActive {
  if (!_isObservingReadingListFolder) {
    return;
  }
  _isObservingReadingListFolder = NO;
  [NSFileCoordinator removeFilePresenter:self];
}

#pragma mark - NSFilePresenter methods

- (void)presentedSubitemDidChangeAtURL:(NSURL*)url {
  if (_taskRunner) {
    __weak ShareExtensionItemReceiver* weakSelf = self;
    _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                            [weakSelf handleFileAtURL:url withCompletion:nil];
                          }));
  }
}

- (NSOperationQueue*)presentedItemOperationQueue {
  return [NSOperationQueue mainQueue];
}

- (NSURL*)presentedItemURL {
  return app_group::ExternalCommandsItemsFolder();
}

@end
