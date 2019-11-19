// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FORMAT_UTILS_H_
#define MEDIA_BASE_FORMAT_UTILS_H_

#include "base/optional.h"
#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "ui/gfx/buffer_types.h"

namespace media {

MEDIA_EXPORT base::Optional<VideoPixelFormat> GfxBufferFormatToVideoPixelFormat(
    gfx::BufferFormat format);

MEDIA_EXPORT base::Optional<gfx::BufferFormat>
VideoPixelFormatToGfxBufferFormat(VideoPixelFormat pixel_format);

}  // namespace media

#endif  // MEDIA_BASE_FORMAT_UTILS_H_
