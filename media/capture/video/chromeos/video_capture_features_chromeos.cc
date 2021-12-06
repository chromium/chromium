// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_features_chromeos.h"

namespace media {

namespace switches {

const char kForceControlFaceAe[] = "force-control-face-ae";
const char kHdrNetOverride[] = "hdrnet-override";
const char kAutoFramingOverride[] = "auto-framing-override";

}  // namespace switches

namespace features {

// Controls if the camera frame is rotated to the upright display orientation in
// the Chrome OS VideoCaptureDevice implementation.
const base::Feature kDisableCameraFrameRotationAtSource{
    "DisableCameraFrameRotationAtSource", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

}  // namespace media
