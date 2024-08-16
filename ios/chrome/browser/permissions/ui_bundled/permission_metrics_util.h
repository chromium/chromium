// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSION_METRICS_UTIL_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSION_METRICS_UTIL_H_

@class PermissionInfo;

// PermissionEvent origins.
enum class PermissionEventOrigin {
  // Event from the permission modal infobar.
  PermissionEventOriginModalInfobar = 0,
  // Event from the page info permission section.
  PermissionEventOriginPageInfo = 1,
  kMaxValue = PermissionEventOriginPageInfo,
};

// Values for UMA permission histograms. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class PermissionEvent {
  cameraPermissionDisabled = 0,
  cameraPermissionEnabled = 1,
  microphonePermissionDisabled = 2,
  microphonePermissionEnabled = 3,
  kMaxValue = microphonePermissionEnabled,
};

// Records metrics related to media permission events.
void RecordPermissionEventFromOrigin(PermissionInfo* permissionInfo,
                                     PermissionEventOrigin permissionOrigin);

// Records a metric when the user toggles a media permission.
void RecordPermissionToogled();

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSION_METRICS_UTIL_H_
