// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_HELPER_H_

#include "base/feature_list.h"
#include "base/time/time.h"

namespace crash_helper {

// Starts the crash handlers. This must be run as soon as possible to catch
// early crashes.
void Start();

// Enables/Disables crash handling.
void SetEnabled(bool enabled);

// Process and begin uploading pending crash reports if application is active.
// If application state is inactive or backgrounded, this is a no-op. Can be
// called multiple times, but will only take effect the first time (when app
// state is active).
void UploadCrashReports();

// Process any pending crashpad reports, and mark them as
// 'uploaded_in_recovery_mode'.
void ProcessIntermediateReportsForSafeMode();

// Returns the number of crash reports waiting to send to the server. This
// function will wait for an operation to complete on a background thread.
int GetPendingCrashReportCount();

// Gets the number of pending crash reports on a background thread and invokes
// `callback` with the result when complete.
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

// Deletes any reports that were recorded or uploaded within the time range.
void ClearReportsBetween(base::Time delete_begin, base::Time delete_end);

}  // namespace crash_helper

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_HELPER_H_
