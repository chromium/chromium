// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_IMAGE_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_IMAGE_UTIL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "media/base/video_transformation.h"
#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_provider.h"
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

// Returns a StaticBitmapImage for the given frame that is either accelerated or
// unaccelerated via exactly one of `snapshot_provider` and `sw_draw_info` being
// valid. If non-null, `snapshot_provider` should have a size equal to
// frame->natural_size() and color space equal to frame->CompatRGBColorSpace().
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
// The client may optionally provide a cached SkSurface for the software draw to
// occur into; if not provided, the software draw will create a new SkSurface to
// draw into.
//
// Returns nullptr if a StaticBitmapImage can't be created.
PLATFORM_EXPORT scoped_refptr<StaticBitmapImage> CreateImageFromVideoFrame(
    scoped_refptr<media::VideoFrame> frame,
    CanvasNon2DResourceProviderSharedImage* snapshot_provider,
    std::optional<CanvasSnapshotProvider::Info> sw_draw_info,
    sk_sp<SkSurface> cached_sw_draw_surface,
    media::PaintCanvasVideoRenderer* video_renderer = nullptr,
    bool prefer_tagged_orientation = true,
    bool reinterpret_video_as_srgb = false);

PLATFORM_EXPORT bool ShouldCreateAcceleratedImages(
    viz::RasterContextProvider* raster_context_provider);

PLATFORM_EXPORT void DrawVideoFrameIntoCanvas(
    scoped_refptr<media::VideoFrame> frame,
    cc::PaintCanvas* canvas,
    const cc::PaintFlags& flags,
    bool ignore_video_transformation = false);

// Extract a RasterContextProvider from the current SharedGpuContext.
PLATFORM_EXPORT scoped_refptr<viz::RasterContextProvider>
GetRasterContextProvider();

// Helper function for creating a CanvasSnapshotProvider from a VideoFrame. The
// returned info structure will be filled as follows:
//   alpha_type: kOpaque_SkAlphaType for opaque frames, kPremul_SkAlphaType
//   otherwise.
//
//   color_space: If `reinterpret_video_as_srgb` was true, then this
//   is sRGB, otherwise frame.CompatRGBColorSpace().
//
//   format: Always GetN32FormatForCanvas() at the time of writing.
//
//   size: Set to frame.natural_size() unless `scaled_size` is provided.
PLATFORM_EXPORT CanvasSnapshotProvider::Info
CreateSnapshotProviderInfoForVideoFrame(
    const media::VideoFrame& frame,
    std::optional<gfx::Size> scaled_size = std::nullopt,
    bool reinterpret_video_as_srgb = false);

// Creates a CanvasSnapshotProvider which is appropriate for drawing VideoFrame
// objects into. Some callers to CreateImageFromVideoFrame() may choose to cache
// their snapshot providers. If `raster_context_provider` is null a software
// snapshot provider will be returned.
PLATFORM_EXPORT std::unique_ptr<CanvasSnapshotProvider>
CreateSnapshotProviderForVideo(
    const CanvasSnapshotProvider::Info& info,
    viz::RasterContextProvider* raster_context_provider = nullptr);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_IMAGE_UTIL_H_
