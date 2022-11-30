// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FORMAT_UTILS_H_
#define MEDIA_BASE_FORMAT_UTILS_H_

#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/buffer_types.h"

namespace media {

MEDIA_EXPORT absl::optional<VideoPixelFormat> GfxBufferFormatToVideoPixelFormat(
    gfx::BufferFormat format);

MEDIA_EXPORT absl::optional<gfx::BufferFormat>
VideoPixelFormatToGfxBufferFormat(VideoPixelFormat pixel_format);

}  // namespace media

#endif  // MEDIA_BASE_FORMAT_UTILS_H_
