// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_STARTUP_INFORMATION_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_STARTUP_INFORMATION_H_

class FirstUserActionRecorder;

namespace base {
class TimeTicks;
}

// Contains information about the startup.
@protocol StartupInformation<NSObject>

// Whether the app is starting in first run.
@property(nonatomic, assign) BOOL isFirstRun;
// Whether the current session began from a cold start. NO if the app has
// entered the background at least once since start up.
@property(nonatomic) BOOL isColdStart;
// Start of the application, used for UMA.
@property(nonatomic, assign) base::TimeTicks appLaunchTime;
// An object to record metrics related to the user's first action.
@property(nonatomic, readonly) FirstUserActionRecorder* firstUserActionRecorder;
// Tick of the call to didFinishLaunching, used for UMA.
@property(nonatomic, assign) base::TimeTicks didFinishLaunchingTime;
// Tick of the first scene connection, used for UMA.
@property(nonatomic, assign) base::TimeTicks firstSceneConnectionTime;

// Disables the FirstUserActionRecorder.
- (void)resetFirstUserActionRecorder;

// Expire the FirstUserActionRecorder and disable it.
- (void)expireFirstUserActionRecorder;

// Expire the FirstUserActionRecorder and disable it after a delay.
- (void)expireFirstUserActionRecorderAfterDelay:(NSTimeInterval)delay;

// Enable the FirstUserActionRecorder with the time spent in background.
- (void)activateFirstUserActionRecorderWithBackgroundTime:
    (NSTimeInterval)backgroundTime;

// Teardown that is needed by common Chrome code. This should not be called if
// Chrome code is still on the stack.
- (void)stopChromeMain;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_STARTUP_INFORMATION_H_
