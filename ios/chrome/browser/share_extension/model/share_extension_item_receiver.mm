// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/share_extension_item_receiver.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/core/reading_list_model_observer.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

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

@interface ShareExtensionItemReceiver () <NSFilePresenter>

// Checks if the reading list folder is already created and if not, create it.
- (void)createReadingListFolder;

// Invoked on UI thread once the reading list folder has been created.
- (void)readingListFolderCreated;

// Processes the data sent by the share extension. Data should be a NSDictionary
// serialized by +|NSKeyedArchiver archivedDataWithRootObject:`.
// `completion` is called if `data` has been fully processed.
- (BOOL)receivedData:(NSData*)data withCompletion:(ProceduralBlock)completion;

// Reads the file pointed by `url` and calls `receivedData:` on the content.
// If the file is processed, delete it.
// `completion` is only called if the file handling is completed without error.
- (void)handleFileAtURL:(NSURL*)url withCompletion:(ProceduralBlock)completion;

// Deletes the file pointed by `url` then call `completion`.
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

@implementation ShareExtensionItemReceiver {
  BOOL _isObservingReadingListFolder;
  BOOL _readingListFolderCreated;
  BOOL _shutdownCalled;
  raw_ptr<ReadingListModel> _readingListModel;
  raw_ptr<bookmarks::BookmarkModel> _bookmarkModel;
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
}

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
  if (![self presentedItemURL]) {
    return nil;
  }

  if (self) {
    _readingListModel = readingListModel;
    _bookmarkModel = bookmarkModel;
    _taskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

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
  _shutdownCalled = YES;
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
    if (_shutdownCalled) {
      return;
    }
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
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf readingListFolderCreated];
      }));
}

- (void)readingListFolderCreated {
  if (_shutdownCalled) {
    return;
  }
  UIApplication* application = [UIApplication sharedApplication];
  if ([application applicationState] == UIApplicationStateActive) {
    _readingListFolderCreated = YES;
    [self applicationDidBecomeActive];
  }
}

- (BOOL)receivedData:(NSData*)data withCompletion:(ProceduralBlock)completion {
  if (_shutdownCalled) {
    return NO;
  }
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
  NSDictionary* entry = base::apple::ObjCCast<NSDictionary>(entryID);
  if (!entry) {
    if (completion) {
      completion();
    }
    return NO;
  }

  NSNumber* cancelled = base::apple::ObjCCast<NSNumber>(
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

  NSURL* entryURL = [entry objectForKey:app_group::kShareItemURL];
  GURL entryGURL = net::GURLWithNSURL(entryURL);
  NSString* entryTitle = [entry objectForKey:app_group::kShareItemTitle];
  NSDate* entryDate = base::apple::ObjCCast<NSDate>(
      [entry objectForKey:app_group::kShareItemDate]);
  NSNumber* entryType = base::apple::ObjCCast<NSNumber>(
      [entry objectForKey:app_group::kShareItemType]);
  NSString* entrySource = base::apple::ObjCCast<NSString>(
      [entry objectForKey:app_group::kShareItemSource]);

  if (!entryGURL.is_valid() || !entrySource || !entryDate || !entryType ||
      !entryGURL.SchemeIsHTTPOrHTTPS()) {
    if (completion) {
      completion();
    }
    return NO;
  }

  UMA_HISTOGRAM_TIMES(
      "IOS.ShareExtension.ReceivedEntryDelay",
      base::Seconds([[NSDate date] timeIntervalSinceDate:entryDate]));

  UMA_HISTOGRAM_ENUMERATION("IOS.ShareExtension.Source",
                            SourceIDFromSource(entrySource),
                            SHARE_EXTENSION_SOURCE_COUNT);

  __weak ShareExtensionItemReceiver* weakSelf = self;
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf processEntryWithType:entryType
                                 title:entryTitle
                                   URL:entryURL
                            completion:completion];
      }));
  return YES;
}

- (void)processEntryWithType:(NSNumber*)entryType
                       title:(NSString*)entryNSTitle
                         URL:(NSURL*)entryNSURL
                  completion:(ProceduralBlock)completion {
  if (_shutdownCalled || !_readingListModel || !_bookmarkModel) {
    // Models may have been deleted after the file
    // processing started.
    return;
  }
  std::string entryTitle = base::SysNSStringToUTF8(entryNSTitle);
  GURL entryURL = net::GURLWithNSURL(entryNSURL);

  app_group::ShareExtensionItemType type =
      static_cast<app_group::ShareExtensionItemType>([entryType integerValue]);
  switch (type) {
    case app_group::READING_LIST_ITEM: {
      LogHistogramReceivedItem(READINGLIST_ENTRY);
      _readingListModel->AddOrReplaceEntry(
          entryURL, entryTitle, reading_list::ADDED_VIA_EXTENSION,
          /*estimated_read_time=*/base::TimeDelta());
      break;
    }
    case app_group::BOOKMARK_ITEM: {
      LogHistogramReceivedItem(BOOKMARK_ENTRY);
      // TODO(crbug.com/40260909): Once feature
      // `syncer::kSyncEnableBookmarksInTransportMode` is launched, this
      // may want to save bookmarks under `_bookmarkModel->mobile_node()`, if
      // it returns non-null.
      _bookmarkModel->AddNewURL(_bookmarkModel->mobile_node(), 0,
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
}

- (void)handleFileAtURL:(NSURL*)url withCompletion:(ProceduralBlock)completion {
  if (_shutdownCalled) {
    return;
  }
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
    if (!weakSelf) {
      return;
    }
    base::ScopedBlockingCall inner_scoped_blocking_call(
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
  if (_shutdownCalled) {
    return;
  }
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  void (^deletingAccessor)(NSURL*) = ^(NSURL* newURL) {
    base::ScopedBlockingCall inner_scoped_blocking_call(
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
  if (_shutdownCalled || !_readingListFolderCreated ||
      _isObservingReadingListFolder) {
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
  if (_shutdownCalled) {
    return;
  }
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
    web::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(^{
                                               [weakSelf entriesReceived:files];
                                             }));
  }
}

- (void)entriesReceived:(NSArray<NSURL*>*)files {
  UMA_HISTOGRAM_COUNTS_100("IOS.ShareExtension.ReceivedEntriesCount",
                           [files count]);
  if (_shutdownCalled || !_taskRunner) {
    return;
  }

  __weak ShareExtensionItemReceiver* weakSelf = self;
  for (NSURL* fileURL : files) {
    __block std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
        batchToken(_readingListModel->BeginBatchUpdates());
    _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                            [weakSelf
                                handleFileAtURL:fileURL
                                 withCompletion:^{
                                   web::GetUIThreadTaskRunner({})->PostTask(
                                       FROM_HERE, base::BindOnce(^{
                                         batchToken.reset();
                                       }));
                                 }];
                          }));
  }
}

- (void)applicationWillResignActive {
  if (_shutdownCalled || !_isObservingReadingListFolder) {
    return;
  }
  _isObservingReadingListFolder = NO;
  [NSFileCoordinator removeFilePresenter:self];
}

#pragma mark - NSFilePresenter methods

- (void)presentedSubitemDidChangeAtURL:(NSURL*)url {
  if (_shutdownCalled) {
    return;
  }
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
