// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_FRAME_RGBA_TO_YUVA_CONVERTER_H_
#define MEDIA_RENDERERS_VIDEO_FRAME_RGBA_TO_YUVA_CONVERTER_H_

#include "media/base/media_export.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"

namespace gfx {
class ColorSpace;
class Size;
}  // namespace gfx

namespace gpu {
struct MailboxHolder;
}  // namespace gpu

namespace viz {
class RasterContextProvider;
class SharedImageFormat;
}  // namespace viz

namespace media {

class VideoFrame;

// Copy the specified source texture to the destination video frame, doing
// color space conversion and RGB to YUV conversion. Waits for all sync
// tokens in `src_mailbox_holder` and `dst_video_frame` before doing the
// copy. Updates `dst_video_frame`'s sync token to wait on copy completion.
MEDIA_EXPORT bool CopyRGBATextureToVideoFrame(
    viz::RasterContextProvider* raster_context_provider,
    viz::SharedImageFormat src_format,
    const gfx::Size& src_size,
    const gfx::ColorSpace& src_color_space,
    GrSurfaceOrigin src_surface_origin,
    const gpu::MailboxHolder& src_mailbox_holder,
    VideoFrame* dst_video_frame);

}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_FRAME_RGBA_TO_YUVA_CONVERTER_H_
