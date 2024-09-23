// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_service_ios.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/files/file_path.h"
#import "base/format_macros.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/logging.h"
#import "base/memory/ref_counted.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/session_internal_util.h"
#import "ios/chrome/browser/sessions/model/session_ios.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"
#import "ios/chrome/browser/sessions/model/session_window_ios_factory.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/web_state_id.h"

namespace {

// Callback invoked to request saving session at path using factory.
using SaveSessionCallback =
    base::RepeatingCallback<void(NSString*, SessionWindowIOSFactory*)>;

}  // namespace

// Represents a pending save request.
@interface SaveSessionRequest : NSObject

// Designated initializer.
- (instancetype)initWithPath:(NSString*)path
                    deadline:(base::TimeTicks)deadline
                     factory:(__weak SessionWindowIOSFactory*)factory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Path at which the data needs to be saved on disk.
@property(nonatomic, readonly) NSString* path;

// Time at which the data needs to be saved on disk.
@property(nonatomic, readonly) base::TimeTicks deadline;

// Factory used to generate the data to save to disk.
@property(nonatomic, weak, readonly) SessionWindowIOSFactory* factory;

@end

@implementation SaveSessionRequest

- (instancetype)initWithPath:(NSString*)path
                    deadline:(base::TimeTicks)deadline
                     factory:(__weak SessionWindowIOSFactory*)factory {
  if ((self = [super init])) {
    DCHECK(path.length);
    _path = [path copy];
    _deadline = deadline;
    _factory = factory;
  }
  return self;
}

@end

// Represents a queue of pending save requests.
@interface SaveSessionRequestQueue : NSObject

// Designated initializer.
- (instancetype)initWithCallback:(SaveSessionCallback)callback
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Called before destroying the task runner.
- (void)shutdown;

// Schedules a new requests to save data at `path` after `delay` using
// `factory`. Ignored if a request scheduled for `path` with a closer
// deadline is already scheduled.
- (void)scheduleRequestForPath:(NSString*)path
                         delay:(base::TimeDelta)delay
                       factory:(__weak SessionWindowIOSFactory*)factory;

@end

@implementation SaveSessionRequestQueue {
  // Priority queue storing the pending requests ordered by deadline.
  std::multimap<base::TimeTicks, SaveSessionRequest*> _priority;

  // Dictionary mapping session path to the pending save request for that path.
  NSMutableDictionary<NSString*, SaveSessionRequest*>* _pending;

  // Callback passed to the constructor and invoked to save a session when
  // the deadline for a request expires.
  SaveSessionCallback _callback;

  // Timer used to wait until the next pending request deadline expires.
  base::OneShotTimer _timer;
}

- (instancetype)initWithCallback:(SaveSessionCallback)callback {
  if ((self = [super init])) {
    _pending = [[NSMutableDictionary alloc] init];
    _callback = std::move(callback);
    DCHECK(!_callback.is_null());
  }
  return self;
}

- (void)shutdown {
  _callback = base::NullCallback();
  _timer.Stop();
}

- (void)scheduleRequestForPath:(NSString*)path
                         delay:(base::TimeDelta)delay
                       factory:(__weak SessionWindowIOSFactory*)factory {
  DCHECK(path.length);
  DCHECK_GE(delay, base::TimeDelta());  // Can't schedule in the past.
  const base::TimeTicks deadline = base::TimeTicks::Now() + delay;
  SaveSessionRequest* request = [_pending objectForKey:path];
  if (request) {
    // The existing request is scheduled with a shorter deadline, ignore the
    // new request. Return early as there is nothing to do.
    if (request.deadline <= deadline) {
      return;
    }

    // Drop the old request as the new one will expire sooner.
    auto range = _priority.equal_range(request.deadline);
    for (auto iter = range.first; iter != range.second; ++iter) {
      if (iter->second == request) {
        _priority.erase(iter);
        break;
      }
    }
    [_pending removeObjectForKey:path];
  }

  request = [[SaveSessionRequest alloc] initWithPath:path
                                            deadline:deadline
                                             factory:factory];

  // Need to reset the timer if the newly scheduled request will have the
  // closest deadline.
  const bool resetTimer =
      _priority.empty() || deadline < _priority.begin()->first;

  [_pending setObject:request forKey:path];
  _priority.insert(std::make_pair(deadline, request));

  if (resetTimer) {
    [self resetTimerWithDelay:delay];
  }
}

// Resets the timer to expire in `delay`. If the delay is zero, then consider
// the timer expires immediately and instead call the timer expiration method.
- (void)resetTimerWithDelay:(base::TimeDelta)delay {
  DCHECK(!_priority.empty());
  DCHECK_GE(delay, base::TimeDelta());
  if (delay == base::TimeDelta()) {
    // No delay, stop the timer and consider it as immediately expired.
    _timer.Stop();
    [self onTimerExpired];
    return;
  }

  __weak SaveSessionRequestQueue* weakSelf = self;
  _timer.Start(FROM_HERE, delay, base::BindOnce(^{
                 [weakSelf onTimerExpired];
               }));
}

// Invoked when the timer expires. Should only happens when the priority queue
// is not empty, and at least one item is scheduled to expire now or in the
// past.
- (void)onTimerExpired {
  const base::TimeTicks now = base::TimeTicks::Now();
  while (!_priority.empty()) {
    auto iter = _priority.begin();
    if (iter->first > now) {
      [self resetTimerWithDelay:(iter->first - now)];
      break;
    }

    SaveSessionRequest* request = iter->second;
    [_pending removeObjectForKey:request.path];
    _priority.erase(iter);

    _callback.Run(request.path, request.factory);
  }
}

@end

@implementation SessionServiceIOS {
  // The SequencedTaskRunner on which File IO operations are performed.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  // Delay before saving data to storage when not saving session immediately.
  base::TimeDelta _saveDelay;

  // Queue of pending save requests.
  SaveSessionRequestQueue* _pendingRequests;
}

#pragma mark - Public interface

- (instancetype)initWithSaveDelay:(base::TimeDelta)saveDelay
                       taskRunner:
                           (const scoped_refptr<base::SequencedTaskRunner>&)
                               taskRunner {
  DCHECK(taskRunner);
  DCHECK_GT(saveDelay, base::Seconds(0));
  self = [super init];
  if (self) {
    _taskRunner = taskRunner;
    _saveDelay = saveDelay;

    __weak SessionServiceIOS* weakSelf = self;
    auto savingBlock = ^(NSString* path, SessionWindowIOSFactory* factory) {
      [weakSelf saveSessionToPath:path usingFactory:factory];
    };

    _pendingRequests = [[SaveSessionRequestQueue alloc]
        initWithCallback:base::BindRepeating(savingBlock)];
  }
  return self;
}

- (void)shutdown {
  [_pendingRequests shutdown];
  _pendingRequests = nil;
  _taskRunner.reset();
}

- (void)shutdownWithClosure:(base::OnceClosure)closure {
  _taskRunner->PostTask(FROM_HERE, std::move(closure));
}

- (void)saveSession:(__weak SessionWindowIOSFactory*)factory
          sessionID:(NSString*)sessionID
          directory:(const base::FilePath&)directory
        immediately:(BOOL)immediately {
  NSString* sessionPath = [[self class] sessionPathForSessionID:sessionID
                                                      directory:directory];

  const base::TimeDelta delay = immediately ? base::TimeDelta() : _saveDelay;
  [_pendingRequests scheduleRequestForPath:sessionPath
                                     delay:delay
                                   factory:factory];
}

- (SessionWindowIOS*)loadSessionWithSessionID:(NSString*)sessionID
                                    directory:(const base::FilePath&)directory {
  NSString* sessionPath = [[self class] sessionPathForSessionID:sessionID
                                                      directory:directory];
  base::TimeTicks start_time = base::TimeTicks::Now();
  SessionWindowIOS* session = [self loadSessionFromPath:sessionPath];
  UmaHistogramTimes("Session.WebStates.ReadFromFileTime",
                    base::TimeTicks::Now() - start_time);
  return session;
}

- (SessionWindowIOS*)loadSessionFromPath:(NSString*)sessionPath {
  SessionWindowIOS* sessionWindowIOS = ios::sessions::ReadSessionWindow(
      base::apple::NSStringToFilePath(sessionPath));

  // If the identifiers loaded from disk are invalid, assign new identifiers.
  for (CRWSessionStorage* sessionStorage in sessionWindowIOS.sessions) {
    if (!sessionStorage.uniqueIdentifier.valid()) {
      sessionStorage.uniqueIdentifier = web::WebStateID::NewUnique();
    }
  }

  return sessionWindowIOS;
}

- (void)deleteSessions:(NSArray<NSString*>*)sessionIDs
             directory:(const base::FilePath&)directory
            completion:(base::OnceClosure)callback {
  NSMutableArray<NSString*>* paths =
      [NSMutableArray arrayWithCapacity:sessionIDs.count];
  for (NSString* sessionID : sessionIDs) {
    [paths addObject:[SessionServiceIOS sessionPathForSessionID:sessionID
                                                      directory:directory]];
  }
  [self deletePaths:paths completion:std::move(callback)];
}

+ (NSString*)sessionPathForSessionID:(NSString*)sessionID
                           directory:(const base::FilePath&)directory {
  DCHECK(sessionID.length != 0);
  return base::apple::FilePathToNSString(
      directory.Append(kLegacySessionsDirname)
          .Append(base::SysNSStringToUTF8(sessionID))
          .Append(kLegacySessionFilename));
}

+ (NSString*)filePathForTabID:(NSString*)tabID
                    sessionID:(NSString*)sessionID
                    directory:(const base::FilePath&)directory {
  return [self filePathForTabID:tabID
                    sessionPath:[self sessionPathForSessionID:sessionID
                                                    directory:directory]];
}

+ (NSString*)filePathForTabID:(NSString*)tabID
                  sessionPath:(NSString*)sessionPath {
  return [NSString stringWithFormat:@"%@-%@", sessionPath, tabID];
}

#pragma mark - Private methods

// Delete files/folders of the given `paths`.
- (void)deletePaths:(NSArray<NSString*>*)paths
         completion:(base::OnceClosure)callback {
  _taskRunner->PostTaskAndReply(
      FROM_HERE, base::BindOnce(^{
        base::ScopedBlockingCall scoped_blocking_call(
            FROM_HERE, base::BlockingType::MAY_BLOCK);
        NSFileManager* fileManager = [NSFileManager defaultManager];
        for (NSString* path : paths) {
          if (![fileManager fileExistsAtPath:path])
            continue;
          [self deleteSessionPaths:path];
        }
      }),
      std::move(callback));
}

- (void)deleteSessionPaths:(NSString*)sessionPath {
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSString* directory = [sessionPath stringByDeletingLastPathComponent];
  NSString* sessionFilename = [sessionPath lastPathComponent];
  NSError* error = nil;
  BOOL isDirectory = NO;
  if (![fileManager fileExistsAtPath:directory isDirectory:&isDirectory] ||
      !isDirectory) {
    return;
  }
  NSArray<NSString*>* fileList =
      [fileManager contentsOfDirectoryAtPath:directory error:&error];
  if (error) {
    CHECK(false) << "Unable to get session path list: "
                 << base::SysNSStringToUTF8(directory) << ": "
                 << base::SysNSStringToUTF8([error description]);
  }
  for (NSString* filename : fileList) {
    if (![filename hasPrefix:sessionFilename]) {
      continue;
    }
    NSString* filepath = [directory stringByAppendingPathComponent:filename];

    if (![fileManager fileExistsAtPath:filepath isDirectory:&isDirectory] ||
        isDirectory) {
      continue;
    }
    if (![fileManager removeItemAtPath:filepath error:&error] || error) {
      CHECK(false) << "Unable to delete path: "
                   << base::SysNSStringToUTF8(filepath) << ": "
                   << base::SysNSStringToUTF8([error description]);
    }
  }
}

// Do the work of saving on a background thread.
- (void)saveSessionToPath:(NSString*)sessionPath
             usingFactory:(SessionWindowIOSFactory*)factory {
  DCHECK(sessionPath);

  // Serialize to NSData on the main thread to avoid accessing potentially
  // non-threadsafe objects on a background thread.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  SessionWindowIOS* sessionWindow = [factory sessionForSaving];

  // Because the factory may be called asynchronously after the underlying
  // web state list is destroyed, the session may be nil; if so, do nothing.
  // Do not record the time spent calling -sessionForSaving: as it not
  // interesting in that case.
  if (!sessionWindow) {
    return;
  }

  @try {
    NSError* error = nil;
    size_t previous_cert_policy_bytes = web::GetCertPolicyBytesEncoded();
    NSData* sessionData =
        [NSKeyedArchiver archivedDataWithRootObject:sessionWindow
                              requiringSecureCoding:NO
                                              error:&error];

    // Store end_time to avoid counting the time spent recording the first
    // metric as part of the second metric recorded (probably negligible).
    const base::TimeTicks end_time = base::TimeTicks::Now();
    base::UmaHistogramTimes(kSessionHistogramSavingTime, end_time - start_time);

    if (!sessionData || error) {
      DLOG(WARNING) << "Error serializing session for path: "
                    << base::SysNSStringToUTF8(sessionPath) << ": "
                    << base::SysNSStringToUTF8([error description]);
      return;
    }

    base::UmaHistogramCounts100000(
        "Session.WebStates.AllSerializedCertPolicyCachesSize",
        web::GetCertPolicyBytesEncoded() - previous_cert_policy_bytes / 1024);

    _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                            [self performSaveSessionData:sessionData
                                             sessionPath:sessionPath];
                          }));
  } @catch (NSException* exception) {
    NOTREACHED_IN_MIGRATION()
        << "Error serializing session for path: "
        << base::SysNSStringToUTF8(sessionPath) << ": "
        << base::SysNSStringToUTF8([exception description]);
    return;
  }
}

@end

@implementation SessionServiceIOS (SubClassing)

- (void)performSaveSessionData:(NSData*)sessionData
                   sessionPath:(NSString*)sessionPath {
  base::ScopedBlockingCall scoped_blocking_call(
            FROM_HERE, base::BlockingType::MAY_BLOCK);

  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSString* directory = [sessionPath stringByDeletingLastPathComponent];

  NSError* error = nil;
  BOOL isDirectory = NO;
  if (![fileManager fileExistsAtPath:directory isDirectory:&isDirectory]) {
    isDirectory = YES;
    if (![fileManager createDirectoryAtPath:directory
                withIntermediateDirectories:YES
                                 attributes:nil
                                      error:&error]) {
      DLOG(WARNING) << "Error creating destination directory: "
                    << base::SysNSStringToUTF8(directory) << ": "
                    << base::SysNSStringToUTF8([error description]);
      return;
    }
  }

  if (!isDirectory) {
    NOTREACHED_IN_MIGRATION() << "Error creating destination directory: "
                              << base::SysNSStringToUTF8(directory) << ": "
                              << "file exists and is not a directory.";
    return;
  }

  NSDataWritingOptions options =
      NSDataWritingAtomic |
      NSDataWritingFileProtectionCompleteUntilFirstUserAuthentication;

  base::TimeTicks start_time = base::TimeTicks::Now();
  if (![sessionData writeToFile:sessionPath options:options error:&error]) {
    DLOG(WARNING) << "Error writing session file: "
                  << base::SysNSStringToUTF8(sessionPath) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return;
  }
  UmaHistogramTimes("Session.WebStates.WriteToFileTime",
                    base::TimeTicks::Now() - start_time);
}

@end
