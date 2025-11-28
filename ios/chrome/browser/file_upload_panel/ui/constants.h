// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_UI_CONSTANTS_H_

// LINT.IfChange(FileUploadPanelContextMenuVariant)
enum class FileUploadPanelContextMenuVariant {
  kPhotoPickerAndCameraAndFilePicker,
  kPhotoPickerAndFilePicker,
  kMaxValue = kPhotoPickerAndFilePicker,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSFileUploadPanelContextMenuVariant)

// LINT.IfChange(FileUploadPanelCameraActionVariant)
// Variants of the camera action in the file upload panel.
// Used for histograms, so do not remove or reorder values.
enum class FileUploadPanelCameraActionVariant {
  kPhotoAndVideo = 0,
  kVideo = 1,
  kPhoto = 2,
  kMaxValue = kPhoto,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSFileUploadPanelCameraActionVariant)

// LINT.IfChange(FileUploadPanelEntryPointVariant)
// Views that can be shown by the file upload panel.
// Used for histograms, so do not remove or reorder values.
enum class FileUploadPanelEntryPointVariant {
  kContextMenu = 0,
  kCamera = 1,
  kFilePicker = 2,
  kMaxValue = kFilePicker,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSFileUploadPanelEntryPointVariant)

// LINT.IfChange(FileUploadPanelContextMenuActionVariant)
// Variants of the context menu action in the file upload panel.
// Used for histograms, so do not remove or reorder values.
enum class FileUploadPanelContextMenuActionVariant {
  kFilePicker = 0,
  kPhotoPicker = 1,
  kCamera = 2,
  kMaxValue = kCamera,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSFileUploadPanelContextMenuActionVariant)

// LINT.IfChange(FileUploadPanelSecurityScopedResourceAccessState)
// States of security scoped resource access.
// Used for histograms, do not reorder.
enum class FileUploadPanelSecurityScopedResourceAccessState {
  kStartedAndStopped = 0,
  kStartFailed = 1,
  kMaxValue = kStartFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSFileUploadPanelSecurityScopedResourceAccessState)

#endif  // IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_UI_CONSTANTS_H_
