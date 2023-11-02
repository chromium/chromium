// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_COMMON_INPUT_ERROR_CODE_CONVERTER_H_
#define MEDIA_MOJO_COMMON_INPUT_ERROR_CODE_CONVERTER_H_

#include "media/base/audio_capturer_source.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace media {
AudioCapturerSource::ErrorCode ConvertToCaptureCallbackCode(
    mojom::InputStreamErrorCode code);
}

#endif  // MEDIA_MOJO_COMMON_INPUT_ERROR_CODE_CONVERTER_H_
