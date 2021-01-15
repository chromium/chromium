// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREAKPAD_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREAKPAD_HELPER_H_

#include <string>

@class NSString;
@class NSArray;

namespace breakpad_helper {

// Starts the crash handlers. This must be run as soon as possible to catch
// early crashes.
void Start(const std::string& channel_name);

// Enables/Disables crash handling.
void SetEnabled(bool enabled);

// Enable/Disable uploading crash reports.
void SetUploadingEnabled(bool enabled);

// Sets the user preferences related to Breakpad and cache them to be used on
// next startup to check if safe mode must be started.
void SetUserEnabledUploading(bool enabled);

// Returns true if uploading crash reports is enabled in the settings.
bool UserEnabledUploading();

// Cleans up all stored crash reports.
void CleanupCrashReports();

// Returns the number of crash reports waiting to send to the server. This
// function will wait for an operation to complete on a background thread.
int GetCrashReportCount();

// Gets the number of crash reports on a background thread and invokes
// |callback| with the result when complete.
void GetCrashReportCount(void (^callback)(int));

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

}  // namespace breakpad_helper

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREAKPAD_HELPER_H_
