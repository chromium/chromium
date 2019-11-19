// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_PREVIOUS_SESSION_INFO_H_
#define IOS_CHROME_BROWSER_METRICS_PREVIOUS_SESSION_INFO_H_

#import <Foundation/Foundation.h>

namespace previous_session_info_constants {
// Key in the UserDefaults for a boolean value keeping track of memory warnings.
extern NSString* const kDidSeeMemoryWarningShortlyBeforeTerminating;
// Key in the UserDefaults for a double value which stores OS start time.
extern NSString* const kOSStartTime;

// The values of this enum are persisted (both to NSUserDefaults and logs) and
// represent the state of the last session (which may have been running a
// different version of the application).
// Therefore, entries should not be renumbered and numeric values should never
// be reused.
enum class DeviceThermalState {
  kUnknown = 0,
  kNominal = 1,
  kFair = 2,
  kSerious = 3,
  kCritical = 4,
  kMaxValue = kCritical,
};

// The values of this enum are persisted (both to NSUserDefaults and logs) and
// represent the state of the last session (which may have been running a
// different version of the application).
// Therefore, entries should not be renumbered and numeric values should never
// be reused.
enum class DeviceBatteryState {
  kUnknown = 0,
  kUnplugged = 1,
  kCharging = 2,
  // Battery is plugged into power and the battery is 100% charged.
  kFull = 3,
  kMaxValue = kFull,
};
}  // namespace previous_session_info_constants

// PreviousSessionInfo has two jobs:
// - Holding information about the last session, persisted across restart.
//   These informations are accessible via the properties on the shared
//   instance.
// - Persist information about the current session, for use in a next session.
@interface PreviousSessionInfo : NSObject

// The battery level of the device at the end of the previous session.
@property(nonatomic, assign, readonly) float deviceBatteryLevel;

// The battery state of the device at the end of the previous session.
@property(nonatomic, assign, readonly)
    previous_session_info_constants::DeviceBatteryState deviceBatteryState;

// The storage available, in kilobytes, at the end of the previous session or -1
// if no previous session data is available.
@property(nonatomic, assign, readonly) NSInteger availableDeviceStorage;

// The thermal state of the device at the end of the previous session.
@property(nonatomic, assign, readonly)
    previous_session_info_constants::DeviceThermalState deviceThermalState;

// Whether the device was in low power mode at the end of the previous session.
@property(nonatomic, assign, readonly) BOOL deviceWasInLowPowerMode;

// Whether the app received a memory warning seconds before being terminated.
@property(nonatomic, assign, readonly)
    BOOL didSeeMemoryWarningShortlyBeforeTerminating;

// Whether or not the system OS was updated between the previous and the
// current session.
@property(nonatomic, assign, readonly) BOOL isFirstSessionAfterOSUpgrade;

// Whether the app was updated between the previous and the current session.
@property(nonatomic, assign, readonly) BOOL isFirstSessionAfterUpgrade;

// Whether the language has been changed between the previous and the current
// session.
@property(nonatomic, assign, readonly) BOOL isFirstSessionAfterLanguageChange;

// Whether or not the OS was restarted between the previous and the current
// session.
@property(nonatomic, assign, readonly) BOOL OSRestartedAfterPreviousSession;

// The OS version during the previous session or nil if no previous session data
// is available.
@property(nonatomic, strong, readonly) NSString* OSVersion;

// The version of the previous session or nil if no previous session data is
// available.
@property(nonatomic, strong, readonly) NSString* previousSessionVersion;

// The time at which the previous sesion ended. Note that this is only an
// estimate and is updated whenever another value of the receiver is updated.
@property(nonatomic, strong, readonly) NSDate* sessionEndTime;

// Singleton PreviousSessionInfo. During the lifetime of the app, the returned
// object is the same, and describes the previous session, even after a new
// session has started (by calling beginRecordingCurrentSession).
+ (instancetype)sharedInstance;

// Clears the persisted information about the previous session and starts
// persisting information about the current session, for use in a next session.
- (void)beginRecordingCurrentSession;

// Updates the currently available device storage, in kilobytes.
- (void)updateAvailableDeviceStorage:(NSInteger)availableStorage;

// Updates the saved last known session time.
- (void)updateSessionEndTime;

// Updates the saved last known battery level of the device.
- (void)updateStoredBatteryLevel;

// Updates the saved last known battery state of the device.
- (void)updateStoredBatteryState;

// Updates the saved last known low power mode setting of the device.
- (void)updateStoredLowPowerMode;

// Updates the saved last known thermal state of the device.
- (void)updateStoredThermalState;

// When a session has begun, records that a memory warning was received.
- (void)setMemoryWarningFlag;

// When a session has begun, records that any memory warning flagged can be
// ignored.
- (void)resetMemoryWarningFlag;

@end

#endif  // IOS_CHROME_BROWSER_METRICS_PREVIOUS_SESSION_INFO_H_
