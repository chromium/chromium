// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_FRAME_YUV_CONVERTER_H_
#define MEDIA_RENDERERS_VIDEO_FRAME_YUV_CONVERTER_H_

#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/media_export.h"
#include "media/base/video_types.h"

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class VideoFrame;
class VideoFrameSharedImageCache;

// Methods are only used by PaintCanvasVideoRenderer.
namespace internals {

MEDIA_EXPORT bool IsPixelFormatSupportedForYuvSharedImageConversion(
    VideoPixelFormat video_format);

// Converts YUV video frames to RGB format and stores the results in the
// provided destination shared image. The caller of this function maintains
// ownership of the destination shared image. Automatically handles upload of
// CPU memory backed VideoFrames in multiplanar format that do not have shared
// image. Converting CPU backed VideoFrames requires creation of YUV shared
// images to upload the frame to the GPU where the conversion takes place. This
// will not perform any color space conversion besides the YUV to RGB conversion
// (it will ignore the color space of the destination shared image). IMPORTANT:
// Callers of this function can pass in `shared_image_cache` to prevent repeated
// creation/deletion of YUV shared images.
MEDIA_EXPORT gpu::SyncToken ConvertYuvVideoFrameToRgbSharedImage(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::Mailbox& dest_mailbox,
    const gpu::SyncToken& dest_sync_token,
    bool use_visible_rect,
    VideoFrameSharedImageCache* shared_image_cache);

}  // namespace internals
}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_FRAME_YUV_CONVERTER_H_
