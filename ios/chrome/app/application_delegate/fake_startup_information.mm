// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/fake_startup_information.h"

#import "base/time/time.h"
#import "ios/chrome/app/app_startup_parameters.h"

@implementation FakeStartupInformation

@synthesize appLaunchTime = _appLaunchTime;
@synthesize didFinishLaunchingTime = _didFinishLaunchingTime;
@synthesize firstSceneConnectionTime = _firstSceneConnectionTime;
@synthesize isFirstRun = _isFirstRun;
@synthesize isColdStart = _isColdStart;

- (FirstUserActionRecorder*)firstUserActionRecorder {
  // Stub.
  return nil;
}

- (void)resetFirstUserActionRecorder {
  // Stub.
}

- (void)expireFirstUserActionRecorderAfterDelay:(NSTimeInterval)delay {
  // Stub.
}

- (void)activateFirstUserActionRecorderWithBackgroundTime:
    (NSTimeInterval)backgroundTime {
  // Stub.
}

- (void)expireFirstUserActionRecorder {
  // Stub.
}

- (void)launchFromURLHandled:(BOOL)URLHandled {
  // Stub.
}

- (void)stopChromeMain {
  // Stub.
}

- (void)startChromeMain {
  // Stub.
}

@end
