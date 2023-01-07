// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCANNER_CAMERA_STATE_H_
#define IOS_CHROME_BROWSER_UI_SCANNER_CAMERA_STATE_H_

namespace scanner {

// Values to distinguish between different camera states to display the correct
// view controller or system alert.
// Note: no state encodes the state where the usage of the camera is prohibited
// because the app is in the background. The reason is that iOS transparently
// stops/starts the camera when the app enter/leaves the background.
// See AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground for
// more information.
enum CameraState {
  // Camera is loaded and available;
  CAMERA_AVAILABLE = 0,
  // The application cannot use the camera because it is in use exclusively by
  // another application.
  CAMERA_IN_USE_BY_ANOTHER_APPLICATION,
  // The application cannot use the camera because video input is not supported
  // if there are multiple foreground apps running.
  MULTIPLE_FOREGROUND_APPS,
  // The application does not have the permission to use the camera.
  CAMERA_PERMISSION_DENIED,
  // Camera unavailable due to "system pressure".
  CAMERA_UNAVAILABLE_DUE_TO_SYSTEM_PRESSURE,
  // The camera is unavailable for an unspecified reason.
  CAMERA_UNAVAILABLE,
  // The camera was not yet loaded.
  CAMERA_NOT_LOADED,
};

}  // namespace scanner

#endif  // IOS_CHROME_BROWSER_UI_SCANNER_CAMERA_STATE_H_
