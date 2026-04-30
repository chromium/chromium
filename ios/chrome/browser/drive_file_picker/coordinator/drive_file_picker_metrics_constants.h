// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_METRICS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_METRICS_CONSTANTS_H_

// The sign in status of the user when they open the drive file picker.
// Enum for the IOS.FilePicker.Drive.SignIn.Status histogram.
// LINT.IfChange(FilePickerDriveSignInStatus)
enum class FilePickerDriveSignInStatus {
  kSignedIn = 0,
  kSignedOutWithoutAccountOnDevice = 1,
  kSignedOutWithAccountOnDevice = 2,
  kMaxValue = kSignedOutWithAccountOnDevice,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSFilePickerDriveSignInStatusType)

// The result of the sign in flow.
// Enum for the IOS.FilePicker.Drive.SignIn.Result histogram.
// LINT.IfChange(FilePickerDriveSignInResult)
enum class FilePickerDriveSignInResult {
  kSignInSuccess = 0,
  kSignInCanceled = 1,
  kSignInFailed = 2,
  kMaxValue = kSignInFailed,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSFilePickerDriveSignInResultType)

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_METRICS_CONSTANTS_H_
