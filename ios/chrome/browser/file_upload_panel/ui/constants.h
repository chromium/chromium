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

#endif  // IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_UI_CONSTANTS_H_
