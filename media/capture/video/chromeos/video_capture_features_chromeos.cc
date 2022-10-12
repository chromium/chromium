// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_features_chromeos.h"

#include "base/command_line.h"

namespace media {

namespace switches {

const char kForceControlFaceAe[] = "force-control-face-ae";
const char kHdrNetOverride[] = "hdrnet-override";
const char kAutoFramingOverride[] = "auto-framing-override";

}  // namespace switches

namespace features {

// Controls if the camera frame is rotated to the upright display orientation in
// the Chrome OS VideoCaptureDevice implementation.
BASE_FEATURE(kDisableCameraFrameRotationAtSource,
             "DisableCameraFrameRotationAtSource",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

// Check if auto framing should be enabled.
bool ShouldEnableAutoFraming() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  // TODO(pihsun): Migrate the flag to use base::Feature.
  std::string value =
      command_line->GetSwitchValueASCII(media::switches::kAutoFramingOverride);
  return value != media::switches::kAutoFramingForceDisabled;
}

}  // namespace media
