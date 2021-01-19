// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_SERVICE_IOS_H_

#import <Foundation/Foundation.h>

#include "base/callback.h"
#include "base/sequenced_task_runner.h"

@class SessionIOS;
@class SessionIOSFactory;

// A singleton service for saving the current session. Can either save on a
// delay or immediately. Saving is always performed on a separate thread.
@interface SessionServiceIOS : NSObject

// Lazily creates a singleton instance with a default task runner.
+ (SessionServiceIOS*)sharedService;

// Initializes a SessionServiceIOS with a given task runner. Prefer to use the
// |sharedService| method.
- (instancetype)initWithTaskRunner:
    (const scoped_refptr<base::SequencedTaskRunner>&)taskRunner
    NS_DESIGNATED_INITIALIZER;

// Saves the session returned by |factory| to |directory|. If |immediately|
// is NO, the save is done after a delay. If another call is pending, this one
// is ignored. If YES, the save is done now, cancelling any pending calls.
// Either way, the save is done on a separate thread to avoid blocking the UI
// thread.
- (void)saveSession:(__weak SessionIOSFactory*)factory
          directory:(NSString*)directory
        immediately:(BOOL)immediately;

// Loads the session from default session file in |directory| on the main
// thread. Returns nil in case of errors.
- (SessionIOS*)loadSessionFromDirectory:(NSString*)directory;

// Loads the session from |sessionPath| on the main thread. Returns nil in case
// of errors.
- (SessionIOS*)loadSessionFromPath:(NSString*)sessionPath;

// Schedules deletion of the all session files from a specific browser state
// |directory|.
- (void)deleteAllSessionFilesInBrowserStateDirectory:(NSString*)directory
                                          completion:
                                              (base::OnceClosure)callback;

// Schedule deletion of session directories with |sessionIDs| which resides in
// a specific browser state |directory|.
- (void)deleteSessions:(NSArray<NSString*>*)sessionIDs
    fromBrowserStateDirectory:(NSString*)directory;

// Returns the path of the session with |sessionID| within a |directory|.
+ (NSString*)sessionPathForSessionID:(NSString*)sessionID
                           directory:(NSString*)directory;

// Returns the path of the session file for |directory|.
+ (NSString*)sessionPathForDirectory:(NSString*)directory;

@end

@interface SessionServiceIOS (SubClassing)

// For some of the unit tests, we need to make sure the session is saved
// immediately so we can read it back in to verify various attributes. This
// is not a situation we normally expect to be in because we never
// want the session being saved on the main thread in the production app.
- (void)performSaveSessionData:(NSData*)sessionData
                   sessionPath:(NSString*)sessionPath;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_SERVICE_IOS_H_
