// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/test_session_service.h"

#import "base/memory/ref_counted.h"
#import "base/threading/thread_task_runner_handle.h"
#import "ios/chrome/browser/sessions/session_ios_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TestSessionService

- (instancetype)init {
  return [super initWithTaskRunner:base::ThreadTaskRunnerHandle::Get()];
}

- (void)saveSession:(__weak SessionIOSFactory*)factory
          sessionID:(NSString*)sessionID
          directory:(const base::FilePath&)directory
        immediately:(BOOL)immediately {
  NSString* sessionPath = [[self class] sessionPathForSessionID:sessionID
                                                      directory:directory];
  NSData* data =
      [NSKeyedArchiver archivedDataWithRootObject:[factory sessionForSaving]
                            requiringSecureCoding:NO
                                            error:nil];
  if (self.performIO)
    [self performSaveSessionData:data tabContents:@{} sessionPath:sessionPath];
  _saveSessionCallsCount++;
}

@end
