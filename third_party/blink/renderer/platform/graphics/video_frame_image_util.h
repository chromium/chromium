// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_IMAGE_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_IMAGE_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/rect.h"

// Note: Don't include "media/base/video_frame.h" here without good reason,
// since it includes a lot of non-blink types which can pollute the namespace.

namespace media {
class PaintCanvasVideoRenderer;
class VideoFrame;
}  // namespace media

namespace viz {
class RasterContextProvider;
}

namespace blink {
class CanvasResourceProvider;
class StaticBitmapImage;

// Returns true if CreateImageFromVideoFrame() expects to create an
// AcceleratedStaticBitmapImage. Note: This may be overridden if a software
// |resource_provider| is given to CreateImageFromVideoFrame().
PLATFORM_EXPORT bool WillCreateAcceleratedImagesFromVideoFrame(
    const media::VideoFrame* frame);

// Returns a StaticBitmapImage for the given frame. Accelerated images will be
// preferred if possible. A zero copy mechanism will be preferred if possible
// unless |allow_zero_copy_images| is false.
//
// |video_renderer| may optionally be provided in cases where the same frame may
// end up repeatedly converted.
//
// Likewise |resource_provider| may be provided to prevent thrashing when this
// method is called with high frequency.
//
// The default resource provider size is the frame's visible size. The default
// |dest_rect| is the visible size aligned to the origin. Callers may choose to
// provide their own |resource_provider| and |dest_rect| for rendering to the
// frame's natural size.
//
// When an external |resource_provider| is provided a |dest_rect| may also be
// provided to control where in the canvas the VideoFrame will be drawn. A
// non-empty |dest_rect| will disable zero copy image support.
PLATFORM_EXPORT scoped_refptr<StaticBitmapImage> CreateImageFromVideoFrame(
    scoped_refptr<media::VideoFrame> frame,
    bool allow_zero_copy_images = true,
    CanvasResourceProvider* resource_provider = nullptr,
    media::PaintCanvasVideoRenderer* video_renderer = nullptr,
    const gfx::Rect& dest_rect = gfx::Rect());

// Similar to the above, but just skips creating the StaticBitmapImage from the
// CanvasResourceProvider. Returns true if the frame could be drawn or false
// otherwise. Note: In certain failure modes a black frame will be drawn.
//
// |video_renderer| may optionally be provided in cases where the same frame may
// end up repeatedly drawn.
//
// A |raster_context_provider| is required to convert texture backed frames.
PLATFORM_EXPORT bool DrawVideoFrameIntoResourceProvider(
    scoped_refptr<media::VideoFrame> frame,
    CanvasResourceProvider* resource_provider,
    viz::RasterContextProvider* raster_context_provider,
    const gfx::Rect& dest_rect,
    media::PaintCanvasVideoRenderer* video_renderer = nullptr);

// Creates a CanvasResourceProvider which is appropriate for drawing VideoFrame
// objects into. Some callers to CreateImageFromVideoFrame() may choose to cache
// their resource providers. If |raster_context_provider| is null a software
// resource provider will be returned.
PLATFORM_EXPORT std::unique_ptr<CanvasResourceProvider>
CreateResourceProviderForVideoFrame(
    IntSize size,
    viz::RasterContextProvider* raster_context_provider);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_IMAGE_UTIL_H_
