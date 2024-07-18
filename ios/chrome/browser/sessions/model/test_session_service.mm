// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/test_session_service.h"

#import "base/memory/ref_counted.h"
#import "base/task/single_thread_task_runner.h"
#import "ios/chrome/browser/sessions/model/session_window_ios_factory.h"

namespace {
constexpr base::TimeDelta kTestSaveDelay = base::Seconds(2.5);
}

@implementation TestSessionService

- (instancetype)init {
  return [super
      initWithSaveDelay:kTestSaveDelay
             taskRunner:base::SingleThreadTaskRunner::GetCurrentDefault()];
}

- (void)saveSession:(__weak SessionWindowIOSFactory*)factory
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
    [self performSaveSessionData:data sessionPath:sessionPath];
  _saveSessionCallsCount++;
}

- (SessionWindowIOS*)loadSessionWithSessionID:(NSString*)sessionID
                                    directory:(const base::FilePath&)directory {
  _loadSessionCallsCount++;
  return [super loadSessionWithSessionID:sessionID directory:directory];
}

@end
