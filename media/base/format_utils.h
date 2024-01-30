// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FORMAT_UTILS_H_
#define MEDIA_BASE_FORMAT_UTILS_H_

#include <optional>

#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "ui/gfx/buffer_types.h"

namespace media {

MEDIA_EXPORT std::optional<VideoPixelFormat> GfxBufferFormatToVideoPixelFormat(
    gfx::BufferFormat format);

MEDIA_EXPORT std::optional<gfx::BufferFormat> VideoPixelFormatToGfxBufferFormat(
    VideoPixelFormat pixel_format);

}  // namespace media

#endif  // MEDIA_BASE_FORMAT_UTILS_H_
