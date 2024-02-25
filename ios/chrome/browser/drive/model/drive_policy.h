// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_POLICY_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_POLICY_H_

// Values for the DownloadManagerSaveToDriveSettings policy.
// VALUES MUST COINCIDE WITH THE DownloadManagerSaveToDriveSettings POLICY
// DEFINITION.
enum class SaveToDrivePolicySettings {
  // The download manager will have an option to save files to Google Drive.
  kEnabled,
  // The download manager will not have an option to save files to Google Drive.
  kDisabled,
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_POLICY_H_
