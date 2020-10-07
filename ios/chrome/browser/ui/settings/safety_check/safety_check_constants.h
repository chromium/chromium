// Copyright 2020 The Chromium Authors. All rights reserved.
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

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_CONSTANTS_H_
