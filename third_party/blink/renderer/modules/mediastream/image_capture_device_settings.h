// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_IMAGE_CAPTURE_DEVICE_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_IMAGE_CAPTURE_DEVICE_SETTINGS_H_

#include <optional>

#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

struct MODULES_EXPORT ImageCaptureDeviceSettings {
  std::optional<double> pan;
  std::optional<double> tilt;
  std::optional<double> zoom;
  std::optional<bool> torch;
  std::optional<bool> background_blur;
  std::optional<bool> eye_gaze_correction;
  std::optional<bool> face_framing;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_IMAGE_CAPTURE_DEVICE_SETTINGS_H_
