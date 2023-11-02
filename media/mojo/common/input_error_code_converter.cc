// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/input_error_code_converter.h"

namespace media {
AudioCapturerSource::ErrorCode ConvertToCaptureCallbackCode(
    mojom::InputStreamErrorCode code) {
  switch (code) {
    case mojom::InputStreamErrorCode::kSystemPermissions:
      return AudioCapturerSource::ErrorCode::kSystemPermissions;
    case mojom::InputStreamErrorCode::kDeviceInUse:
      return AudioCapturerSource::ErrorCode::kDeviceInUse;
    case mojom::InputStreamErrorCode::kUnknown:
      break;
  }
  return AudioCapturerSource::ErrorCode::kUnknown;
}
}  // namespace media
