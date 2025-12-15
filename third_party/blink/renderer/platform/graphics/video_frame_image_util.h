// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_IMAGE_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_IMAGE_UTIL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "media/base/video_transformation.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

// Note: Don't include "media/base/video_frame.h" here without good reason,
// since it includes a lot of non-blink types which can pollute the namespace.
namespace media {
class PaintCanvasVideoRenderer;
class VideoFrame;
}  // namespace media

namespace viz {
class RasterContextProvider;
}

namespace cc {
class PaintCanvas;
class PaintFlags;
}  // namespace cc

namespace blink {
class CanvasSnapshotProvider;
class StaticBitmapImage;

// Converts a media orientation into a blink one or vice versa.
PLATFORM_EXPORT ImageOrientationEnum
VideoTransformationToImageOrientation(media::VideoTransformation transform);
PLATFORM_EXPORT media::VideoTransformation
ImageOrientationToVideoTransformation(ImageOrientationEnum orientation);

// Returns true if CreateImageFromVideoFrame() expects to create an
// AcceleratedStaticBitmapImage. Note: This may be overridden if a software
// `snapshot_provider` is given to CreateImageFromVideoFrame().
PLATFORM_EXPORT bool WillCreateAcceleratedImagesFromVideoFrame();

// Returns a StaticBitmapImage for the given frame. Accelerated images will be
// preferred if possible. `snapshot_provider` must be non-null and should have a
// size equal to frame->natural_size() and color space equal to
// frame->CompatRGBColorSpace().
//
// `video_renderer` may optionally be provided in cases where the same frame may
// end up repeatedly converted.
//
// If `prefer_tagged_orientation` is true, CreateImageFromVideoFrame() will just
// tag the StaticBitmapImage with the correct orientation ("soft flip") instead
// of drawing the frame with the correct orientation ("hard flip").
//
// If `reinterpret_video_as_srgb` true, then the video will be reinterpreted as
// being originally having been in sRGB.
//
// Returns nullptr if a StaticBitmapImage can't be created.
PLATFORM_EXPORT scoped_refptr<StaticBitmapImage> CreateImageFromVideoFrame(
    scoped_refptr<media::VideoFrame> frame,
    CanvasSnapshotProvider* snapshot_provider,
    media::PaintCanvasVideoRenderer* video_renderer = nullptr,
    bool prefer_tagged_orientation = true,
    bool reinterpret_video_as_srgb = false);

PLATFORM_EXPORT void DrawVideoFrameIntoCanvas(
    scoped_refptr<media::VideoFrame> frame,
    cc::PaintCanvas* canvas,
    const cc::PaintFlags& flags,
    bool ignore_video_transformation = false);

// Extract a RasterContextProvider from the current SharedGpuContext.
PLATFORM_EXPORT scoped_refptr<viz::RasterContextProvider>
GetRasterContextProvider();

// Creates a CanvasSnapshotProvider which is appropriate for drawing VideoFrame
// objects into. Some callers to CreateImageFromVideoFrame() may choose to cache
// their snapshot providers. If `raster_context_provider` is null a software
// snapshot provider will be returned.
PLATFORM_EXPORT std::unique_ptr<CanvasSnapshotProvider>
CreateSnapshotProviderForVideoFrame(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    viz::RasterContextProvider* raster_context_provider);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_IMAGE_UTIL_H_
