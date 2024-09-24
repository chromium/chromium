// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/web_session_state_cache.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/base_paths.h"
#import "base/containers/contains.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/observer_list.h"
#import "base/path_service.h"
#import "base/sequence_checker.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/web_session_state_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state_id.h"

namespace {

// The delay, in seconds, for cleaning up any unassociated session state files
// when -removeSessionStateDataForWebState is called while `_delayRemove` is
// true.
const int kRemoveSessionStateDataDelay = 10;

// Returns the session identifier for `web_state_id` as a string.
std::string SessionIdentifierForWebStateID(web::WebStateID web_state_id) {
  DCHECK(web_state_id.valid());
  DCHECK_GT(web_state_id.identifier(), 0);

  static_assert(sizeof(decltype(web_state_id.identifier())) == sizeof(int32_t));
  const uint32_t identifier = static_cast<uint32_t>(web_state_id.identifier());

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

  NSString* file_path_string = base::apple::FilePathToNSString(file_path);
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
// The ProfileIOS passed on initialization.
@property(nonatomic, assign) ProfileIOS* profile;
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

- (instancetype)initWithBrowserState:(ProfileIOS*)profile {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ((self = [super init])) {
    _profile = profile;
    _cacheDirectory = profile->GetStatePath().Append(kLegacyWebSessionsDirname);
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
                  forWebStateID:(web::WebStateID)webStateID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!data || !_taskRunner) {
    return;
  }

  _taskRunner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WriteSessionData, data,
          _cacheDirectory.Append(SessionIdentifierForWebStateID(webStateID))));
}

- (NSData*)sessionStateDataForWebStateID:(web::WebStateID)webStateID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return [NSData dataWithContentsOfFile:base::apple::FilePathToNSString(
                                            _cacheDirectory.Append(
                                                SessionIdentifierForWebStateID(
                                                    webStateID)))];
}

- (void)purgeUnassociatedDataWithCompletion:(base::OnceClosure)closure {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self purgeCacheExcept:[self liveSessionIDs] closure:std::move(closure)];
}

- (void)removeSessionStateDataForWebStateID:(web::WebStateID)webStateID
                                  incognito:(BOOL)incognito {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner)
    return;

  if (_delayRemove && !incognito) {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self performSelector:@selector(purgeUnassociatedData)
               withObject:nil
               afterDelay:kRemoveSessionStateDataDelay];
    return;
  }

  _taskRunner->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&base::DeleteFile),
          _cacheDirectory.Append(SessionIdentifierForWebStateID(webStateID))));
}

- (void)setDelayRemove:(BOOL)delayRemove {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _delayRemove = delayRemove;
}

- (void)shutdown {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  _taskRunner = nullptr;
  _profile = nullptr;
}

#pragma mark - Private

- (void)purgeUnassociatedData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self purgeUnassociatedDataWithCompletion:base::DoNothing()];
}

// Returns a set of all known tab ids.
- (std::set<std::string>)liveSessionIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(_profile) << "-liveSessionIDs called after -shutdown";

  std::set<std::string> liveSessionIDs;
  BrowserList* browserList = BrowserListFactory::GetForProfile(self.profile);
  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kAll)) {
    WebStateList* webStateList = browser->GetWebStateList();
    for (int index = 0; index < webStateList->count(); ++index) {
      web::WebState* webState = webStateList->GetWebStateAt(index);
      liveSessionIDs.insert(
          SessionIdentifierForWebStateID(webState->GetUniqueIdentifier()));
    }
  }

  return liveSessionIDs;
}

// Deletes any files from the session cache directory that don't exist in
// `liveSessionIDs`.
- (void)purgeCacheExcept:(std::set<std::string>)liveSessionIDs
                 closure:(base::OnceClosure)closure {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
    return;
  }

  std::set<base::FilePath> filesToKeep;
  for (const std::string& sessionID : liveSessionIDs) {
    filesToKeep.insert(_cacheDirectory.Append(sessionID));
  }

  _taskRunner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PurgeCacheOnBackgroundSequenceExcept, _cacheDirectory,
                     filesToKeep),
      std::move(closure));
}

@end
