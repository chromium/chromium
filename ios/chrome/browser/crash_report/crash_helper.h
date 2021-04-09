// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_HELPER_H_

#include <string>

namespace crash_helper {

// Sync the kCrashpadIOS feature to kCrashpadStartOnNextRun NSUserDefault.
void SyncCrashpadEnabledOnNextRun();

// Starts the crash handlers. This must be run as soon as possible to catch
// early crashes.
void Start();

// Enables/Disables crash handling.
void SetEnabled(bool enabled);

// Enable/Disable uploading crash reports.
void SetUploadingEnabled(bool enabled);

// Sets the user preferences related to Breakpad and cache them to be used on
// next startup to check if safe mode must be started.
void SetUserEnabledUploading(bool enabled);

// For breakpad, if |after_upgrade| is true, delete all pending reports.  For
// crashpad, regardless of |after_upgrade|, process pending intermediate dumps.
void CleanupCrashReports(bool after_upgrade);

// Process any pending crashpad reports, and mark them as
// 'uploaded_in_recovery_mode'.
void ProcessIntermediateReportsForSafeMode();

// Returns the number of crash reports waiting to send to the server. This
// function will wait for an operation to complete on a background thread.
int GetPendingCrashReportCount();

// Gets the number of pending crash reports on a background thread and invokes
// |callback| with the result when complete.
void GetPendingCrashReportCount(void (^callback)(int));

// Check if there is currently a crash report to upload. This function will wait
// for an operation to complete on a background thread.
bool HasReportToUpload();

// Informs the crash report helper that crash restoration is about to begin.
void WillStartCrashRestoration();

// Starts uploading crash reports. Sets the upload interval to 1 second, and
// sets a key in uploaded reports to allow tracking of reports that are uploaded
// in recovery mode.
void StartUploadingReportsInRecoveryMode();

// Resets the Breakpad configuration from the main bundle.
void RestoreDefaultConfiguration();

}  // namespace crash_helper

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_HELPER_H_
