// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_service_ios.h"

#import <UIKit/UIKit.h>

#import "base/files/file_path.h"
#import "base/format_macros.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/memory/ref_counted.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/time/time.h"
#import "ios/chrome/browser/sessions/scene_util.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_ios_factory.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/session/crw_session_storage.h"

namespace {
const NSTimeInterval kSaveDelay = 2.5;     // Value taken from Desktop Chrome.
NSString* const kRootObjectKey = @"root";  // Key for the root object.
}

@implementation NSKeyedUnarchiver (CrLegacySessionCompatibility)

// When adding a new compatibility alias here, create a new crbug to track its
// removal and mark it with a release at least one year after the introduction
// of the alias.
- (void)cr_registerCompatibilityAliases {
}

@end

@implementation SessionServiceIOS {
  // The SequencedTaskRunner on which File IO operations are performed.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  // Maps session path to the pending session factories for the delayed save
  // behaviour. SessionIOSFactory pointers are weak.
  NSMapTable<NSString*, SessionIOSFactory*>* _pendingSessions;
}

#pragma mark - NSObject overrides

- (instancetype)init {
  scoped_refptr<base::SequencedTaskRunner> taskRunner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  return [self initWithTaskRunner:taskRunner];
}

#pragma mark - Public interface

+ (SessionServiceIOS*)sharedService {
  static SessionServiceIOS* singleton = nil;
  if (!singleton) {
    singleton = [[[self class] alloc] init];
  }
  return singleton;
}

- (instancetype)initWithTaskRunner:
    (const scoped_refptr<base::SequencedTaskRunner>&)taskRunner {
  DCHECK(taskRunner);
  self = [super init];
  if (self) {
    _pendingSessions = [NSMapTable strongToWeakObjectsMapTable];
    _taskRunner = taskRunner;
  }
  return self;
}

- (void)shutdownWithCompletion:(ProceduralBlock)completion {
  _taskRunner->PostTask(FROM_HERE, base::BindOnce(completion));
}

- (void)saveSession:(__weak SessionIOSFactory*)factory
          sessionID:(NSString*)sessionID
          directory:(const base::FilePath&)directory
        immediately:(BOOL)immediately {
  NSString* sessionPath = [[self class] sessionPathForSessionID:sessionID
                                                      directory:directory];
  BOOL hadPendingSession = [_pendingSessions objectForKey:sessionPath] != nil;
  [_pendingSessions setObject:factory forKey:sessionPath];
  if (immediately) {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self performSaveToPathInBackground:sessionPath];
  } else if (!hadPendingSession) {
    // If there wasn't previously a delayed save pending for `sessionPath`,
    // enqueue one now.
    [self performSelector:@selector(performSaveToPathInBackground:)
               withObject:sessionPath
               afterDelay:kSaveDelay];
  }
}

- (SessionIOS*)loadSessionWithSessionID:(NSString*)sessionID
                              directory:(const base::FilePath&)directory {
  NSString* sessionPath = [[self class] sessionPathForSessionID:sessionID
                                                      directory:directory];
  base::TimeTicks start_time = base::TimeTicks::Now();
  SessionIOS* session = [self loadSessionFromPath:sessionPath];
  UmaHistogramTimes("Session.WebStates.ReadFromFileTime",
                    base::TimeTicks::Now() - start_time);
  return session;
}

- (SessionIOS*)loadSessionFromPath:(NSString*)sessionPath {
  NSObject<NSCoding>* rootObject = nil;
  @try {
    NSData* data = [NSData dataWithContentsOfFile:sessionPath];
    if (!data)
      return nil;

    NSError* error = nil;
    NSKeyedUnarchiver* unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    if (!unarchiver || error) {
      DLOG(WARNING) << "Error creating unarchiver, session file: "
                    << base::SysNSStringToUTF8(sessionPath) << ": "
                    << base::SysNSStringToUTF8([error description]);
      return nil;
    }

    unarchiver.requiresSecureCoding = NO;

    // Register compatibility aliases to support legacy saved sessions.
    [unarchiver cr_registerCompatibilityAliases];
    rootObject = [unarchiver decodeObjectForKey:kRootObjectKey];
  } @catch (NSException* exception) {
    NOTREACHED() << "Error loading session file: "
                 << base::SysNSStringToUTF8(sessionPath) << ": "
                 << base::SysNSStringToUTF8([exception reason]);
  }

  if (!rootObject)
    return nil;

  // Support for legacy saved session that contained a single SessionWindowIOS
  // object as the root object (pre-M-59).
  if ([rootObject isKindOfClass:[SessionWindowIOS class]]) {
    return [[SessionIOS alloc] initWithWindows:@[
      base::mac::ObjCCastStrict<SessionWindowIOS>(rootObject)
    ]];
  }

  return base::mac::ObjCCastStrict<SessionIOS>(rootObject);
}

- (void)deleteAllSessionFilesInDirectory:(const base::FilePath&)directory
                              completion:(base::OnceClosure)callback {
  NSString* sessionsDirectory =
      base::mac::FilePathToNSString(SessionsDirectoryForDirectory(directory));
  NSArray<NSString*>* allSessionIDs = [[NSFileManager defaultManager]
      contentsOfDirectoryAtPath:sessionsDirectory
                          error:nil];

  [self deleteSessions:allSessionIDs
             directory:directory
            completion:std::move(callback)];
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
  return base::mac::FilePathToNSString(
      SessionPathForDirectory(directory, sessionID, kSessionFileName));
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
- (void)performSaveToPathInBackground:(NSString*)sessionPath {
  DCHECK(sessionPath);

  const base::TimeTicks start_time = base::TimeTicks::Now();

  // Serialize to NSData on the main thread to avoid accessing potentially
  // non-threadsafe objects on a background thread.
  SessionIOSFactory* factory = [_pendingSessions objectForKey:sessionPath];
  [_pendingSessions removeObjectForKey:sessionPath];
  SessionIOS* session = [factory sessionForSaving];

  // Because the factory may be called asynchronously after the underlying
  // web state list is destroyed, the session may be nil; if so, do nothing.
  // Do not record the time spent calling -sessionForSaving: as it not
  // interesting in that case.
  if (!session)
    return;

  @try {
    NSError* error = nil;
    size_t previous_cert_policy_bytes = web::GetCertPolicyBytesEncoded();
    const base::TimeTicks archiving_start_time = base::TimeTicks::Now();
    NSData* sessionData = [NSKeyedArchiver archivedDataWithRootObject:session
                                                requiringSecureCoding:NO
                                                                error:&error];

    // Store end_time to avoid counting the time spent recording the first
    // metric as part of the second metric recorded (probably negligible).
    const base::TimeTicks end_time = base::TimeTicks::Now();
    base::UmaHistogramTimes("Session.WebStates.SavingTimeOnMainThread",
                            end_time - start_time);
    base::UmaHistogramTimes("Session.WebStates.ArchivedDataWithRootObjectTime",
                            end_time - archiving_start_time);

    if (!sessionData || error) {
      DLOG(WARNING) << "Error serializing session for path: "
                    << base::SysNSStringToUTF8(sessionPath) << ": "
                    << base::SysNSStringToUTF8([error description]);
      return;
    }

    base::UmaHistogramCounts100000(
        "Session.WebStates.AllSerializedCertPolicyCachesSize",
        web::GetCertPolicyBytesEncoded() - previous_cert_policy_bytes / 1024);

    base::UmaHistogramCounts100000("Session.WebStates.SerializedSize",
                                   sessionData.length / 1024);

    _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                            [self performSaveSessionData:sessionData
                                             sessionPath:sessionPath];
                          }));
  } @catch (NSException* exception) {
    NOTREACHED() << "Error serializing session for path: "
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
    NOTREACHED() << "Error creating destination directory: "
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
