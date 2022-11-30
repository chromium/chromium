// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/public/cros_features.h"

#include "base/files/file_util.h"

namespace media {

bool ShouldUseCrosCameraService() {
  // Checks whether the Chrome OS binary which provides the HAL v3 camera
  // service is installed on the device.  If the binary exists we assume the
  // device is using the new camera HAL v3 stack.
  const base::FilePath kCrosCameraService("/usr/bin/cros_camera_service");
  return base::PathExists(kCrosCameraService);
}

}  // namespace media
