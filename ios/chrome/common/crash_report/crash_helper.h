// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CRASH_REPORT_CRASH_HELPER_H_
#define IOS_CHROME_COMMON_CRASH_REPORT_CRASH_HELPER_H_

#include "base/files/file_path.h"

namespace crash_helper {
namespace common {

// Key in NSUserDefaults for a Boolean value that stores whether to upload
// crash reports.
extern const char kCrashReportsUploadingEnabledKey[];

// Key in NSUserDefaults for a Boolean value that stores the last feature
// value of kCrashpadIOS.
extern const char kCrashpadStartOnNextRun[];

// Returns true if uploading crash reports is enabled in the settings.
bool UserEnabledUploading();

// Check and cache the GroupUserDefaults value synced from the associated
// kCrashpadIOS feature.
bool CanCrashpadStart();

// Returns the shared app group crashpad directory.
base::FilePath CrashpadDumpLocation();

// Initialize Crashpad.
void StartCrashpad();

}  // namespace common
}  // namespace crash_helper

#endif  // IOS_CHROME_COMMON_CRASH_REPORT_CRASH_HELPER_H_
