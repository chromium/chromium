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

CAPTURE_EXPORT extern const char kAutoFramingOverride[];
inline constexpr char kAutoFramingForceEnabled[] = "force-enabled";
inline constexpr char kAutoFramingForceDisabled[] = "force-disabled";

CAPTURE_EXPORT extern const char kCameraSuperResOverride[];
inline constexpr char kCameraSuperResForceEnabled[] = "force-enabled";
inline constexpr char kCameraSuperResForceDisabled[] = "force-disabled";

CAPTURE_EXPORT extern const char kFaceRetouchOverride[];
inline constexpr char kFaceRetouchForceEnabledWithRelighting[] =
    "force-enabled-with-relighting";
inline constexpr char kFaceRetouchForceEnabledWithoutRelighting[] =
    "force-enabled-without-relighting";
inline constexpr char kFaceRetouchForceDisabled[] = "force-disabled";

}  // namespace switches

namespace features {

CAPTURE_EXPORT BASE_DECLARE_FEATURE(kDisableCameraFrameRotationAtSource);

}  // namespace features

CAPTURE_EXPORT bool ShouldEnableAutoFraming();

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_FEATURES_CHROMEOS_H_
