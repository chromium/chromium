// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CRASH_REPORT_CRASH_HELPER_H_
#define IOS_CHROME_COMMON_CRASH_REPORT_CRASH_HELPER_H_

#include "base/files/file_path.h"

namespace crash_helper {
namespace common {

// Returns true if uploading crash reports is enabled in the settings.
bool UserEnabledUploading();

// Sets the user preferences related to uploading crash reports and cache them
// to be used on next startup to check if safe mode must be started.
void SetUserEnabledUploading(bool enabled);

// Returns the shared app group crashpad directory.
base::FilePath CrashpadDumpLocation();

// Initialize Crashpad.
bool StartCrashpad();

}  // namespace common
}  // namespace crash_helper

#endif  // IOS_CHROME_COMMON_CRASH_REPORT_CRASH_HELPER_H_
