// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_PIXEL_FORMAT_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_PIXEL_FORMAT_UTILS_H_

#include <optional>
#include <vector>

#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/buffer_types.h"

namespace media {

// A collection of the various pixel formats we need to look up.  We need to
// resolve the HAL pixel format to VideoPixelFormat for VideoCaptureDevice, and
// to gfx::BufferFormat for gpu::GpuMemoryBufferManager.
struct ChromiumPixelFormat {
  VideoPixelFormat video_format;
  gfx::BufferFormat gfx_format;
};

// Converts the HAL pixel format |from| to Chromium pixel format.  Returns
// empty vector if |from| is not supported.
std::vector<ChromiumPixelFormat> PixFormatHalToChromium(
    cros::mojom::HalPixelFormat from);

// Converts the video pixel format |from| to DRM pixel format.  Returns 0
// if |from| is not supported.
uint32_t PixFormatVideoToDrm(VideoPixelFormat from);

// Converts the video pixel format |pixel_format| to gfx::BufferFormat.
std::optional<gfx::BufferFormat> PixFormatVideoToGfx(
    VideoPixelFormat pixel_format);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_PIXEL_FORMAT_UTILS_H_
