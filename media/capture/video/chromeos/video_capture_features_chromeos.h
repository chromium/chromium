// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_FEATURES_CHROMEOS_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_FEATURES_CHROMEOS_H_

#include "base/feature_list.h"
#include "media/capture/capture_export.h"

namespace media {

namespace switches {

CAPTURE_EXPORT extern const char kForceControlFaceAe[];

CAPTURE_EXPORT extern const char kHdrNetOverride[];
constexpr char kHdrNetForceEnabled[] = "force-enabled";
constexpr char kHdrNetForceDisabled[] = "force-disabled";

CAPTURE_EXPORT extern const char kAutoFramingOverride[];
constexpr char kAutoFramingForceEnabled[] = "force-enabled";
constexpr char kAutoFramingForceDisabled[] = "force-disabled";

}  // namespace switches

namespace features {

CAPTURE_EXPORT BASE_DECLARE_FEATURE(kDisableCameraFrameRotationAtSource);

}  // namespace features

CAPTURE_EXPORT bool ShouldEnableAutoFraming();

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_FEATURES_CHROMEOS_H_
