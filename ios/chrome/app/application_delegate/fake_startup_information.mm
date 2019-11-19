// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/application_delegate/fake_startup_information.h"

#include "base/time/time.h"
#import "ios/chrome/app/app_startup_parameters.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeStartupInformation

@synthesize appLaunchTime = _appLaunchTime;
@synthesize isPresentingFirstRunUI = _isPresentingFirstRunUI;
@synthesize isColdStart = _isColdStart;
@synthesize startupParameters = _startupParameters;

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
