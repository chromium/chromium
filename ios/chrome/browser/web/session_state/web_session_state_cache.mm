// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"

#import <UIKit/UIKit.h>

#import "base/base_paths.h"
#import "base/bind.h"
#import "base/containers/contains.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/observer_list.h"
#import "base/path_service.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::FilePath::CharType kWebSessionCacheDirectoryName[] =
    FILE_PATH_LITERAL("Web_Sessions");

namespace {

// The delay, in seconds, for cleaning up any unassociated session state files
// when -removeSessionStateDataForWebState is called while `_delayRemove` is
// true.
const int kRemoveSessionStateDataDelay = 10;

// Writes `sessionData` to `cacheDirectory`.  If -writeToFile fails, deletes
// the old (now stale) data.
void WriteSessionData(NSData* sessionData,
                      base::FilePath cacheDirectory,
                      NSString* sessionID) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::DirectoryExists(cacheDirectory)) {
    bool success = base::CreateDirectory(cacheDirectory);
    if (!success) {
      DLOG(ERROR) << "Error creating session cache directory "
                  << cacheDirectory.AsUTF8Unsafe();
      return;
    }
  }

  NSDataWritingOptions options =
      NSDataWritingAtomic |
      NSDataWritingFileProtectionCompleteUntilFirstUserAuthentication;

  base::FilePath filePath =
      cacheDirectory.Append(base::SysNSStringToUTF8(sessionID));
  NSString* filePathString = base::SysUTF8ToNSString(filePath.AsUTF8Unsafe());
  NSError* error = nil;
  if (![sessionData writeToFile:filePathString options:options error:&error]) {
    NOTREACHED() << "Error writing session data: "
                 << base::SysNSStringToUTF8(filePathString) << ": "
                 << base::SysNSStringToUTF8([error description]);
    // If -writeToFile failed, this webState's session data is now stale.
    // Delete it and revert to legacy session restore.
    base::DeleteFile(filePath);
    return;
  }
}

// Helper function to implement -purgeCacheExcept: on a background sequence.
void PurgeCacheOnBackgroundSequenceExcept(
    const base::FilePath& cache_directory,
    const std::set<base::FilePath>& files_to_keep) {
  if (!base::DirectoryExists(cache_directory))
    return;

  base::FileEnumerator enumerator(cache_directory, false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath current_file = enumerator.Next(); !current_file.empty();
       current_file = enumerator.Next()) {
    if (base::Contains(files_to_keep, current_file))
      continue;
    base::DeleteFile(current_file);
  }
}

}  // anonymous namespace

@interface WebSessionStateCache ()
// The ChromeBrowserState passed on initialization.
@property(nonatomic) ChromeBrowserState* browserState;
@end

@implementation WebSessionStateCache {
  // Task runner used to run tasks in the background. Will be invalidated when
  // -shutdown is invoked. Code should support this value to be null (generally
  // by not posting the task).
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  // Directory where the thumbnails are saved.
  base::FilePath _cacheDirectory;

  // When set, delay calls to -removeSessionStateDataForWebState, replaced with
  // a single -purgeCache call.
  BOOL _delayRemove;

  // Check that public API is called from the correct sequence.
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ((self = [super init])) {
    _browserState = browserState;
    _cacheDirectory =
        browserState->GetStatePath().Append(kWebSessionCacheDirectoryName);
    _taskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_taskRunner) << "-shutdown must be called before -dealloc";
}

- (void)persistSessionStateData:(NSData*)data
                    forWebState:(const web::WebState*)webState {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSString* sessionID = webState->GetStableIdentifier();
  if (!data || !sessionID || !_taskRunner)
    return;

  // Copy ivars used by the block so that it does not reference `self`.
  const base::FilePath cacheDirectory = _cacheDirectory;

  // Save the session to disk.
  _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                          WriteSessionData(data, cacheDirectory, sessionID);
                        }));
}

- (NSData*)sessionStateDataForWebState:(const web::WebState*)webState {
  NSString* sessionID = webState->GetStableIdentifier();
  base::FilePath filePath =
      _cacheDirectory.Append(base::SysNSStringToUTF8(sessionID));
  NSString* filePathString = base::SysUTF8ToNSString(filePath.AsUTF8Unsafe());
  return [NSData dataWithContentsOfFile:filePathString];
}

- (void)purgeUnassociatedData {
  if (!_taskRunner)
    return;
  [self purgeCacheExcept:[self liveSessionIDs]];
}

- (void)removeSessionStateDataForWebState:(const web::WebState*)webState {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  if (!_taskRunner)
    return;

  if (_delayRemove && !webState->GetBrowserState()->IsOffTheRecord()) {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self performSelector:@selector(purgeUnassociatedData)
               withObject:nil
               afterDelay:kRemoveSessionStateDataDelay];
    return;
  }

  NSString* sessionID = webState->GetStableIdentifier();

  base::FilePath filePath =
      _cacheDirectory.Append(base::SysNSStringToUTF8(sessionID));
  _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                          base::DeleteFile(filePath);
                        }));
}

- (void)setDelayRemove:(BOOL)delayRemove {
  _delayRemove = delayRemove;
}

- (void)shutdown {
  _taskRunner = nullptr;
  _browserState = nullptr;
}

#pragma mark - Private

// Returns a set of all known tab ids.
- (NSSet*)liveSessionIDs {
  DCHECK(_browserState) << "-liveSessionIDs called after -shutdown";

  NSMutableSet* liveSessionIDs = [[NSMutableSet alloc] init];
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(self.browserState);
  for (Browser* browser : browserList->AllRegularBrowsers()) {
    WebStateList* webStateList = browser->GetWebStateList();
    for (int index = 0; index < webStateList->count(); ++index) {
      web::WebState* webState = webStateList->GetWebStateAt(index);
      [liveSessionIDs addObject:webState->GetStableIdentifier()];
    }
  }

  for (Browser* browser : browserList->AllIncognitoBrowsers()) {
    WebStateList* webStateList = browser->GetWebStateList();
    for (int index = 0; index < webStateList->count(); ++index) {
      web::WebState* webState = webStateList->GetWebStateAt(index);
      [liveSessionIDs addObject:webState->GetStableIdentifier()];
    }
  }
  return liveSessionIDs;
}

// Deletes any files from the session cache directory that don't exist in
// `liveSessionIDs`.
- (void)purgeCacheExcept:(NSSet*)liveSessionIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  if (!_taskRunner)
    return;

  std::set<base::FilePath> filesToKeep;
  for (NSString* sessionID : liveSessionIDs) {
    filesToKeep.insert(
        _cacheDirectory.Append(base::SysNSStringToUTF8(sessionID)));
  }

  _taskRunner->PostTask(FROM_HERE,
                        base::BindOnce(&PurgeCacheOnBackgroundSequenceExcept,
                                       _cacheDirectory, filesToKeep));
}

@end
