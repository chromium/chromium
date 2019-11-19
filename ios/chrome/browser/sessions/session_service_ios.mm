// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_service_ios.h"

#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/session/crw_session_storage.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

namespace {
const NSTimeInterval kSaveDelay = 2.5;     // Value taken from Desktop Chrome.
NSString* const kRootObjectKey = @"root";  // Key for the root object.
}

@implementation NSKeyedUnarchiver (CrLegacySessionCompatibility)

// When adding a new compatibility alias here, create a new crbug to track its
// removal and mark it with a release at least one year after the introduction
// of the alias.
- (void)cr_registerCompatibilityAliases {
  // TODO(crbug.com/661633): those aliases where introduced between M57 and
  // M58, so remove them after M67 has shipped to stable.
  [self setClass:[CRWSessionCertificatePolicyCacheStorage class]
      forClassName:@"SessionCertificatePolicyManager"];
  [self setClass:[CRWSessionStorage class] forClassName:@"SessionController"];
  [self setClass:[CRWSessionStorage class]
      forClassName:@"CRWSessionController"];
  [self setClass:[CRWNavigationItemStorage class] forClassName:@"SessionEntry"];
  [self setClass:[CRWNavigationItemStorage class]
      forClassName:@"CRWSessionEntry"];
  [self setClass:[SessionWindowIOS class] forClassName:@"SessionWindow"];

  // TODO(crbug.com/661633): this alias was introduced between M58 and M59, so
  // remove it after M68 has shipped to stable.
  [self setClass:[CRWSessionStorage class]
      forClassName:@"CRWNavigationManagerStorage"];
  [self setClass:[CRWSessionCertificatePolicyCacheStorage class]
      forClassName:@"CRWSessionCertificatePolicyManager"];
}

@end

@implementation SessionServiceIOS {
  // The SequencedTaskRunner on which File IO operations are performed.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  // Maps session path to the pending session for the delayed save behaviour.
  NSMutableDictionary<NSString*, SessionIOSFactory>* _pendingSessions;
}

#pragma mark - NSObject overrides

- (instancetype)init {
  scoped_refptr<base::SequencedTaskRunner> taskRunner =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
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
    _pendingSessions = [NSMutableDictionary dictionary];
    _taskRunner = taskRunner;
  }
  return self;
}

- (void)saveSession:(SessionIOSFactory)factory
          directory:(NSString*)directory
        immediately:(BOOL)immediately {
  NSString* sessionPath = [[self class] sessionPathForDirectory:directory];
  BOOL hadPendingSession = [_pendingSessions objectForKey:sessionPath] != nil;
  [_pendingSessions setObject:factory forKey:sessionPath];
  if (immediately) {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self performSaveToPathInBackground:sessionPath];
  } else if (!hadPendingSession) {
    // If there wasn't previously a delayed save pending for |sessionPath|,
    // enqueue one now.
    [self performSelector:@selector(performSaveToPathInBackground:)
               withObject:sessionPath
               afterDelay:kSaveDelay];
  }
}

- (SessionIOS*)loadSessionFromDirectory:(NSString*)directory {
  NSString* sessionPath = [[self class] sessionPathForDirectory:directory];
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

- (void)deleteLastSessionFileInDirectory:(NSString*)directory
                              completion:(base::OnceClosure)callback {
  NSString* sessionPath = [[self class] sessionPathForDirectory:directory];
  _taskRunner->PostTaskAndReply(
      FROM_HERE, base::BindOnce(^{
        base::ScopedBlockingCall scoped_blocking_call(
            FROM_HERE, base::BlockingType::MAY_BLOCK);
        NSFileManager* fileManager = [NSFileManager defaultManager];
        if (![fileManager fileExistsAtPath:sessionPath])
          return;

        NSError* error = nil;
        if (![fileManager removeItemAtPath:sessionPath error:&error] || error) {
          CHECK(false) << "Unable to delete session file: "
                       << base::SysNSStringToUTF8(sessionPath) << ": "
                       << base::SysNSStringToUTF8([error description]);
        }
      }),
      std::move(callback));
}

+ (NSString*)sessionPathForDirectory:(NSString*)directory {
  return [directory stringByAppendingPathComponent:@"session.plist"];
}

#pragma mark - Private methods

// Do the work of saving on a background thread.
- (void)performSaveToPathInBackground:(NSString*)sessionPath {
  DCHECK(sessionPath);
  DCHECK([_pendingSessions objectForKey:sessionPath] != nil);

  // Serialize to NSData on the main thread to avoid accessing potentially
  // non-threadsafe objects on a background thread.
  SessionIOSFactory factory = [_pendingSessions objectForKey:sessionPath];
  [_pendingSessions removeObjectForKey:sessionPath];
  SessionIOS* session = factory();
  // Because the factory may be called asynchronously after the underlying
  // web state list is destroyed, the session may be nil; if so, do nothing.
  if (!session)
    return;

  @try {
    NSError* error = nil;
    NSData* sessionData = [NSKeyedArchiver archivedDataWithRootObject:session
                                                requiringSecureCoding:NO
                                                                error:&error];
    if (!sessionData || error) {
      DLOG(WARNING) << "Error serializing session for path: "
                    << base::SysNSStringToUTF8(sessionPath) << ": "
                    << base::SysNSStringToUTF8([error description]);
      return;
    }

    UMA_HISTOGRAM_COUNTS_100000("Session.WebStates.SerializedSize",
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
      NOTREACHED() << "Error creating destination directory: "
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
      NSDataWritingAtomic | NSDataWritingFileProtectionComplete;

  base::TimeTicks start_time = base::TimeTicks::Now();
  if (![sessionData writeToFile:sessionPath options:options error:&error]) {
    NOTREACHED() << "Error writing session file: "
                 << base::SysNSStringToUTF8(sessionPath) << ": "
                 << base::SysNSStringToUTF8([error description]);
    return;
  }
  UmaHistogramTimes("Session.WebStates.WriteToFileTime",
                    base::TimeTicks::Now() - start_time);
}

@end
