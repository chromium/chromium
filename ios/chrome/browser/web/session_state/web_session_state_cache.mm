// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"

#import <UIKit/UIKit.h>

#import "base/base_paths.h"
#import "base/containers/contains.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/observer_list.h"
#import "base/path_service.h"
#import "base/sequence_checker.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"

const base::FilePath::CharType kWebSessionCacheDirectoryName[] =
    FILE_PATH_LITERAL("Web_Sessions");

namespace {

// The delay, in seconds, for cleaning up any unassociated session state files
// when -removeSessionStateDataForWebState is called while `_delayRemove` is
// true.
const int kRemoveSessionStateDataDelay = 10;

// Returns the session identifier for `web_state` as a string.
std::string SessionIdentifierForWebState(const web::WebState* web_state) {
  DCHECK(web_state->GetUniqueIdentifier().is_valid());
  DCHECK_GT(web_state->GetUniqueIdentifier().id(), 0);

  static_assert(sizeof(SessionID::id_type) == sizeof(int32_t));
  const uint32_t identifier =
      static_cast<uint32_t>(web_state->GetUniqueIdentifier().id());

  return base::StringPrintf("%08u", identifier);
}

// Writes `session_data` to `file_path`.  If -writeToFile fails, deletes
// the old (now stale) data.
void WriteSessionData(NSData* session_data, base::FilePath file_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const base::FilePath directory = file_path.DirName();
  if (!base::DirectoryExists(directory)) {
    bool success = base::CreateDirectory(directory);
    if (!success) {
      DLOG(ERROR) << "Error creating session cache directory "
                  << directory.AsUTF8Unsafe();
      return;
    }
  }

  NSDataWritingOptions options =
      NSDataWritingAtomic |
      NSDataWritingFileProtectionCompleteUntilFirstUserAuthentication;

  NSString* file_path_string = base::mac::FilePathToNSString(file_path);
  NSError* error = nil;
  if (![session_data writeToFile:file_path_string
                         options:options
                           error:&error]) {
    DLOG(WARNING) << "Error writing session data: "
                  << base::SysNSStringToUTF8(file_path_string) << ": "
                  << base::SysNSStringToUTF8([error description]);
    // If -writeToFile failed, this session data is now stale. Delete it and
    // revert to legacy session restore.
    base::DeleteFile(file_path);
    return;
  }
}

// Helper function to implement -purgeCacheExcept: on a background sequence.
void PurgeCacheOnBackgroundSequenceExcept(
    base::FilePath cache_directory,
    std::set<base::FilePath> files_to_keep) {
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
  if (!data || !_taskRunner) {
    return;
  }

  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&WriteSessionData, data,
                                _cacheDirectory.Append(
                                    SessionIdentifierForWebState(webState))));
}

- (NSData*)sessionStateDataForWebState:(const web::WebState*)webState {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  const base::FilePath filePath =
      _cacheDirectory.Append(SessionIdentifierForWebState(webState));
  NSData* data =
      [NSData dataWithContentsOfFile:base::mac::FilePathToNSString(filePath)];

  if (!data) {
    // Until M-115, the file name was derived from GetStableIdentifier()
    // instead of GetUniqueIndentifier(). So if the session cannot be loaded
    // with the new path, check if it exists with the old path, and then
    // rename the file.
    //
    // This code is only reachable if new serialisation code has never been
    // enabled (because otherwise the file would have been migrated already)
    // and thus GetStableIdentifier() is still valid and can be used to load
    // the file.
    const base::FilePath alternateFilePath = _cacheDirectory.Append(
        base::SysNSStringToUTF8(webState->GetStableIdentifier()));

    data = [NSData dataWithContentsOfFile:base::mac::FilePathToNSString(
                                              alternateFilePath)];
    if (data && _taskRunner) {
      _taskRunner->PostTask(FROM_HERE,
                            base::BindOnce(base::IgnoreResult(&base::Move),
                                           alternateFilePath, filePath));
    }
  }

  return data;
}

- (void)purgeUnassociatedData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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

  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                _cacheDirectory.Append(
                                    SessionIdentifierForWebState(webState))));
}

- (void)setDelayRemove:(BOOL)delayRemove {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _delayRemove = delayRemove;
}

- (void)shutdown {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _taskRunner = nullptr;
  _browserState = nullptr;
}

#pragma mark - Private

// Returns a set of all known tab ids.
- (std::set<std::string>)liveSessionIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(_browserState) << "-liveSessionIDs called after -shutdown";

  std::set<std::string> liveSessionIDs;
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(self.browserState);
  for (Browser* browser : browserList->AllRegularBrowsers()) {
    WebStateList* webStateList = browser->GetWebStateList();
    for (int index = 0; index < webStateList->count(); ++index) {
      web::WebState* webState = webStateList->GetWebStateAt(index);
      liveSessionIDs.insert(SessionIdentifierForWebState(webState));

      // Since until M-115, the filename was derived from GetStableIdentifier()
      // and since the file are renamed only when loaded (which happens only
      // for realized WebState), we have to also preserve any file named after
      // GetStableIdentifier().
      //
      // The file are renamed when loaded, so eventually those paths won't
      // correspond to existing files.
      liveSessionIDs.insert(
          base::SysNSStringToUTF8(webState->GetStableIdentifier()));
    }
  }

  for (Browser* browser : browserList->AllIncognitoBrowsers()) {
    WebStateList* webStateList = browser->GetWebStateList();
    for (int index = 0; index < webStateList->count(); ++index) {
      web::WebState* webState = webStateList->GetWebStateAt(index);
      liveSessionIDs.insert(SessionIdentifierForWebState(webState));

      // Since until M-115, the filename was derived from GetStableIdentifier()
      // and since the file are renamed only when loaded (which happens only
      // for realized WebState), we have to also preserve any file named after
      // GetStableIdentifier().
      //
      // The file are renamed when loaded, so eventually those paths won't
      // correspond to existing files.
      liveSessionIDs.insert(
          base::SysNSStringToUTF8(webState->GetStableIdentifier()));
    }
  }
  return liveSessionIDs;
}

// Deletes any files from the session cache directory that don't exist in
// `liveSessionIDs`.
- (void)purgeCacheExcept:(std::set<std::string>)liveSessionIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner)
    return;

  std::set<base::FilePath> filesToKeep;
  for (const std::string& sessionID : liveSessionIDs) {
    filesToKeep.insert(_cacheDirectory.Append(sessionID));
  }

  _taskRunner->PostTask(FROM_HERE,
                        base::BindOnce(&PurgeCacheOnBackgroundSequenceExcept,
                                       _cacheDirectory, filesToKeep));
}

@end
