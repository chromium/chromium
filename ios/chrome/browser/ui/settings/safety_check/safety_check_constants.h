// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_CONSTANTS_H_

#import <Foundation/Foundation.h>

// UMA histogram values for Safety check interactions. Some value don't apply to
// iOS. Note: this should stay in sync with SettingsSafetyCheckInteractions in
// enums.xml.
enum class SafetyCheckInteractions {
  kStarted = 0,
  kUpdatesRelaunch = 1,
  kPasswordsManage = 2,
  kSafeBrowsingManage = 3,
  kExtensionsReview = 4,
  kChromeCleanerReboot = 5,
  kChromeCleanerReview = 6,
  // New elements go above.
  kMaxValue = kChromeCleanerReview,
};

// Address of page with safebrowsing settings pages.
extern NSString* const kSafeBrowsingSafetyCheckStringURL;

// The NSUserDefaults key for the timestamp of last time safety check found an
// issue.
extern NSString* const kTimestampOfLastIssueFoundKey;

// Enum with all possible states of the update check.
typedef NS_ENUM(NSInteger, UpdateCheckRowStates) {
  // When the user is up to date.
  UpdateCheckRowStateUpToDate,
  // When the check has not been run yet.
  UpdateCheckRowStateDefault,
  // When the user is out of date.
  UpdateCheckRowStateOutOfDate,
  // When the user is managed.
  UpdateCheckRowStateManaged,
  // When the check is running.
  UpdateCheckRowStateRunning,
  // When Omaha encountered an error.
  UpdateCheckRowStateOmahaError,
  // When there is a connectivity issue.
  UpdateCheckRowStateNetError,
  // When the device is on a non-supported channel.
  UpdateCheckRowStateChannel,
};

// Enum with all possible states of the password check.
typedef NS_ENUM(NSInteger, PasswordCheckRowStates) {
  // When no compromised passwords were detected.
  PasswordCheckRowStateSafe,
  // When user has unmuted compromised passwords.
  PasswordCheckRowStateUnmutedCompromisedPasswords,
  // When user has reused passwords.
  PasswordCheckRowStateReusedPasswords,
  // When user has weak passwords.
  PasswordCheckRowStateWeakPasswords,
  // When user has dismissed warnings.
  PasswordCheckRowStateDismissedWarnings,
  // When check has not been run yet.
  PasswordCheckRowStateDefault,
  // When password check is running.
  PasswordCheckRowStateRunning,
  // When user has no passwords and check can't be performed.
  PasswordCheckRowStateDisabled,
  // When password check failed due to network issues, quota limit or others.
  PasswordCheckRowStateError,
};

// Enum with all possible states of the Safe Browsing check.
typedef NS_ENUM(NSInteger, SafeBrowsingCheckRowStates) {
  // When check was not run yet.
  SafeBrowsingCheckRowStateDefault,
  // When Safe Browsing is managed by admin.
  SafeBrowsingCheckRowStateManaged,
  // When the Safe Browsing check is running.
  SafeBrowsingCheckRowStateRunning,
  // When Safe Browsing is enabled.
  SafeBrowsingCheckRowStateSafe,
  // When Safe Browsing is disabled.
  SafeBrowsingCheckRowStateUnsafe,
};

// Enum with all possible states of the button to start the check.
typedef NS_ENUM(NSInteger, CheckStartStates) {
  // When the check is not running.
  CheckStartStateDefault,
  // When the check is running.
  CheckStartStateCancel,
};

// Name of the histogram used for recording the resulting state of the updates
// check.
extern const char kSafetyCheckMetricsUpdates[];

// Name of the histogram used for recording the resulting state of the password
// check.
extern const char kSafetyCheckMetricsPasswords[];

// Name of the histogram used for recording the resulting state of the Safe
// Browsing check.
extern const char kSafetyCheckMetricsSafeBrowsing[];

// Name of the histogram used for recording safety check interactions.
extern const char kSafetyCheckInteractions[];

// Accessibility identifier for the Check Now button in the Safety Check module.
extern NSString* const kSafetyCheckCheckNowButtonAccessibilityID;

// Accessibility identifier for the button on the Safety Check page that lets
// users opt in to receive notifications.
extern NSString* const kSafetyCheckNotificationsOptInButtonAccessibilityID;

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_CONSTANTS_H_
