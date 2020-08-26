// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_YUV_UTIL_H_
#define MEDIA_RENDERERS_YUV_UTIL_H_

#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"

// Skia forward declarations
class GrBackendTexture;
class GrDirectContext;
class SkImage;

namespace gpu {
struct MailboxHolder;
}

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class VideoFrame;

// Converts a YUV video frame to RGB format and stores the results in the
// provided mailbox. The caller of this function maintains ownership of the
// mailbox. Automatically handles upload of CPU memory backed VideoFrames in
// I420 format. VideoFrames that wrap external textures can be I420 or NV12
// format.
MEDIA_EXPORT void ConvertFromVideoFrameYUV(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder);

MEDIA_EXPORT sk_sp<SkImage> NewSkImageFromVideoFrameYUV(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    unsigned int texture_target,
    unsigned int texture_id);

MEDIA_EXPORT sk_sp<SkImage> YUVGrBackendTexturesToSkImage(
    GrDirectContext* gr_context,
    gfx::ColorSpace video_color_space,
    VideoPixelFormat video_format,
    GrBackendTexture* yuv_textures,
    const GrBackendTexture& result_texture);

}  // namespace media

#endif  // MEDIA_RENDERERS_YUV_UTIL_H_