// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREAKPAD_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREAKPAD_HELPER_H_

#include <string>

@class NSString;

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

// Add a key/value pair the next crash report. If async is false, this function
// will wait until the key is registered before returning.
void AddReportParameter(NSString* key, NSString* value, bool async);

// Remove the key/value pair associated to key from the next crash report.
void RemoveReportParameter(NSString* key);

// Returns the number of crash reports waiting to send to the server. This
// function will wait for an operation to complete on a background thread.
int GetCrashReportCount();

// Gets the number of crash reports on a background thread and invokes
// |callback| with the result when complete.
void GetCrashReportCount(void (^callback)(int));

// Check if there is currently a crash report to upload. This function will wait
// for an operation to complete on a background thread.
bool HasReportToUpload();

// Sets a key if |background| is true, unset if false. This will allow tracking
// of crashes that occur when the app is backgrounded.
void SetCurrentlyInBackground(bool background);

// Sets a key if |signedIn| is true, unset if false. The key indicates that the
// user is signed-in.
void SetCurrentlySignedIn(bool signedIn);

// Sets a key to indicate the number of memory warnings the application has
// received over its lifetime, or unset the key if the count is zero.
void SetMemoryWarningCount(int count);

// Sets a key indicating a memory warning is deemed to be in progress (if value
// is 'true'), otherwise remove the key.
void SetMemoryWarningInProgress(bool value);

// Sets a key indicating that UI thread is frozen (if value is 'true'),
// otherwise remove the key.
void SetHangReport(bool value);

// Sets a key indicating the current free memory amount in KB. 0 does not remove
// the key as getting no memory is important information.
void SetCurrentFreeMemoryInKB(int value);

// Sets a key indicating the current free disk space in KB. 0 does not remove
// the key as getting no free disk space is important information.
void SetCurrentFreeDiskInKB(int value);

// Increases a key indicating the number of PDF tabs opened. If value is TRUE,
// the counter is increased. If value is FALSE, the counter is decreased. If
// counter falls to 0, the entry is removed. This function does not keep
// previous state. If SetCurrentTabIsPDF is called twice with TRUE, the counter
// will be incremented twice.
void SetCurrentTabIsPDF(bool value);

// Sets a key in browser_state dictionary to store the device orientation.
// Each values is 1 digit: first is the UI orientation from the Foundation
// UIInterfaceOrientation enum (values decimal from 1 to 4) and the second is
// the device orientation with values from the Foundation UIDeviceOrientation
// enum (values decimal from 0 to 7).
void SetCurrentOrientation(int statusBarOrientation, int deviceOrientation);

// Sets a key in browser_state dictionary to store the device horizontal size
// class. The values are from the UIKit UIUserInterfaceSizeClass enum (decimal
// values from 0 to 2).
void SetCurrentHorizontalSizeClass(int horizontalSizeClass);

// Sets a key in browser_state dictionary to store the device user interface
// style. The values are from the UIKit UIUserInterfaceStyle enum (decimal
// values from 0 to 2).
void SetCurrentUserInterfaceStyle(int userInterfaceStyle);

// Sets a key in browser_state dictionary to store the count of regular tabs.
void SetRegularTabCount(int tabCount);

// Sets a key in browser_state dictionary to store the count of incognito tabs.
void SetIncognitoTabCount(int tabCount);

// Sets a key indicating that destroying and rebuilding the incognito browser
// state is in progress, otherwise remove the key.
void SetDestroyingAndRebuildingIncognitoBrowserState(bool in_progress);

// Sets a key to help debug a crash when animating from grid to visible tab.
// |to_view_controller| is the view controller about to be presented. The
// remaining parameters relate to the |to_view_controller|.
void SetGridToVisibleTabAnimation(NSString* to_view_controller,
                                  NSString* presenting_view_controller,
                                  NSString* presented_view_controller,
                                  NSString* parent_view_controller);

// Removes the key to help debug a crash when animating from grid to visible
// tab.
void RemoveGridToVisibleTabAnimation();

// Sets a key in browser to store the playback state of media player (audio or
// video). This function records a new start. This function is called for each
// stream in the media (once or twice for audio, two or three times for video).
void MediaStreamPlaybackDidStart();

// Sets a key in browser to store the playback state of media player (audio or
// video). This function records a stop or pause. This function must be called
// the same number of times as MediaStreamPlaybackDidStart.
void MediaStreamPlaybackDidStop();

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
