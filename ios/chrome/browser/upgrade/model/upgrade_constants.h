// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPGRADE_MODEL_UPGRADE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UPGRADE_MODEL_UPGRADE_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Values of UMA IOS.UpgradeCenter.UpgradeFailed histograms. Entries should not
// be renumbered and numeric values should never be reused.
enum class UpgradeCenterFailureReason {
  kInvalidURL = 0,
  kInvalidVersion = 1,
  kMaxValue = kInvalidVersion,
};

// The Pref key for the upgrade version.
extern const char kIOSChromeNextVersionKey[];
// The Pref key for the upgrade URL.
extern const char kIOSChromeUpgradeURLKey[];
// The user defaults key for up to date status;
extern NSString* const kIOSChromeUpToDateKey;
// The Pref key for the last time the update infobar was shown.
extern const char kLastInfobarDisplayTimeKey[];

#endif  // IOS_CHROME_BROWSER_UPGRADE_MODEL_UPGRADE_CONSTANTS_H_
