// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_IMAGE_CAPTURE_DEVICE_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_IMAGE_CAPTURE_DEVICE_SETTINGS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

struct MODULES_EXPORT ImageCaptureDeviceSettings {
  absl::optional<double> pan;
  absl::optional<double> tilt;
  absl::optional<double> zoom;
  absl::optional<bool> torch;
  absl::optional<bool> background_blur;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_IMAGE_CAPTURE_DEVICE_SETTINGS_H_
