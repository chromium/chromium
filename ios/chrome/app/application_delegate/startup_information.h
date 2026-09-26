// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_STARTUP_INFORMATION_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_STARTUP_INFORMATION_H_

#import <optional>

class FirstUserActionRecorder;

namespace base {
class TimeDelta;
class TimeTicks;
}

// LINT.IfChange(IOSLaunchReason)
// Enum representing the reason why Chrome was launched on iOS.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IOSLaunchReason {
  // The process was pre-warmed by iOS and subsequently brought to the
  // foreground without having performed an intervening background task. If a
  // pre-warmed launch is used to perform a background task, the background
  // launch reason supersedes pre-warming.
  kPreWarming = 0,
  kForeground = 1,
  kBackgroundRefresh = 2,
  kBackgroundURLSession = 3,
  kBackgroundSilentNotification = 4,
  // The launch reason could not be determined within the expected timeout
  // delay. A large number of counts in this bucket is a sign of either
  // unusually slow launches or a background launch use case that is not being
  // properly detected by our startup metrics.
  kSuspicious = 5,
  kMaxValue = kSuspicious,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/startup/enums.xml:IOSLaunchReason)

// Returns whether `reason` represents a background launch reason.
inline constexpr bool IsBackgroundLaunchReason(IOSLaunchReason reason) {
  switch (reason) {
    case IOSLaunchReason::kBackgroundRefresh:
    case IOSLaunchReason::kBackgroundURLSession:
    case IOSLaunchReason::kBackgroundSilentNotification:
      return true;
    case IOSLaunchReason::kPreWarming:
    case IOSLaunchReason::kForeground:
    case IOSLaunchReason::kSuspicious:
      return false;
  }
}

// Contains information about the startup.
@protocol StartupInformation <NSObject>

// Whether the app is starting in first run.
@property(nonatomic, assign) BOOL isFirstRun;
// Whether the current session began from a cold start. NO if the app has
// entered the background at least once since start up.
@property(nonatomic, assign) BOOL isColdStart;
// The initial launch reason for the application process, or std::nullopt if
// not yet determined.
@property(nonatomic, readonly) std::optional<IOSLaunchReason> launchReason;
// Whether the application was launched into the background. Returns YES if
// `launchReason` is any of the background launch reasons (refresh, URL session,
// silent notification), or NO otherwise.
@property(nonatomic, readonly) BOOL isLaunchedInBackground;
// YES if the application is getting terminated.
@property(nonatomic, readonly) BOOL isTerminating;
// Start of the application, used for UMA.
@property(nonatomic, assign) base::TimeTicks appLaunchTime;
// The duration between process creation and the call to main, used for UMA.
@property(nonatomic, readonly) base::TimeDelta preMainDuration;
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

// Sets the initial launch reason if not already determined.
- (void)maybeSetLaunchReason:(IOSLaunchReason)launchReason;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_STARTUP_INFORMATION_H_
