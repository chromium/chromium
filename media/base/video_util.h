// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_UTIL_H_
#define MEDIA_BASE_VIDEO_UTIL_H_

#include <stdint.h>

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "media/base/encoder_status.h"
#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class TimeDelta;
}

namespace gpu {
struct Capabilities;
namespace raster {
class RasterInterface;
}  // namespace raster
}  // namespace gpu

namespace libyuv {
struct YuvConstants;
}

namespace media {

class VideoFramePool;
class VideoFrame;

// Fills |frame| containing YUV data to the given color values.
MEDIA_EXPORT void FillYUV(VideoFrame* frame, uint8_t y, uint8_t u, uint8_t v);

// Fills |frame| containing YUVA data with the given color values.
MEDIA_EXPORT void FillYUVA(VideoFrame* frame,
                           uint8_t y,
                           uint8_t u,
                           uint8_t v,
                           uint8_t a);

// Creates a border in |frame| such that all pixels outside of |view_area| are
// black. Only YV12 and ARGB format video frames are currently supported. If
// format is YV12, the size and position of |view_area| must be even to align
// correctly with the color planes.
MEDIA_EXPORT void LetterboxVideoFrame(VideoFrame* frame,
                                      const gfx::Rect& view_area);

// Rotates |src| plane by |rotation| degree with possible flipping vertically
// and horizontally.
// |rotation| is limited to {0, 90, 180, 270}.
// |width| and |height| are expected to be even numbers.
// Both |src| and |dest| planes are packed and have same |width| and |height|.
// When |width| != |height| and rotated by 90/270, only the maximum square
// portion located in the center is rotated. For example, for width=640 and
// height=480, the rotated area is 480x480 located from row 0 through 479 and
// from column 80 through 559. The leftmost and rightmost 80 columns are
// ignored for both |src| and |dest|.
// The caller is responsible for blanking out the margin area.
MEDIA_EXPORT void RotatePlaneByPixels(const uint8_t* src,
                                      uint8_t* dest,
                                      int width,
                                      int height,
                                      int rotation,  // Clockwise.
                                      bool flip_vert,
                                      bool flip_horiz);

// Return the largest centered rectangle with the same aspect ratio of |content|
// that fits entirely inside of |bounds|.  If |content| is empty, its aspect
// ratio would be undefined; and in this case an empty Rect would be returned.
MEDIA_EXPORT gfx::Rect ComputeLetterboxRegion(const gfx::Rect& bounds,
                                              const gfx::Size& content);

// Same as ComputeLetterboxRegion(), except ensure the result has even-numbered
// x, y, width, and height. |bounds| must already have even-numbered
// coordinates, but the |content| size can be anything.
//
// This is useful for ensuring content scaled and converted to I420 does not
// have color distortions around the edges in a letterboxed video frame. Note
// that, in cases where ComputeLetterboxRegion() would return a 1x1-sized Rect,
// this function could return either a 0x0-sized Rect or a 2x2-sized Rect.
// Note that calling this function with `bounds` that already have the aspect
// ratio of `content` is not guaranteed to be a no-op (for context, see
// https://crbug.com/1323367).
MEDIA_EXPORT gfx::Rect ComputeLetterboxRegionForI420(const gfx::Rect& bounds,
                                                     const gfx::Size& content);

// Shrinks the given |rect| by the minimum amount necessary to align its corners
// to even-numbered coordinates. |rect| is assumed to have bounded limit values,
// and may have negative bounds.
MEDIA_EXPORT gfx::Rect MinimallyShrinkRectForI420(const gfx::Rect& rect);

// Return a scaled |size| whose area is less than or equal to |target|, where
// one of its dimensions is equal to |target|'s.  The aspect ratio of |size| is
// preserved as closely as possible.  If |size| is empty, the result will be
// empty.
MEDIA_EXPORT gfx::Size ScaleSizeToFitWithinTarget(const gfx::Size& size,
                                                  const gfx::Size& target);

// Return a scaled |size| whose area is greater than or equal to |target|, where
// one of its dimensions is equal to |target|'s.  The aspect ratio of |size| is
// preserved as closely as possible.  If |size| is empty, the result will be
// empty.
MEDIA_EXPORT gfx::Size ScaleSizeToEncompassTarget(const gfx::Size& size,
                                                  const gfx::Size& target);

// Calculates the largest sub-rectangle of a rectangle of size |size| with
// roughly the same aspect ratio as |target| and centered both horizontally
// and vertically within the rectangle. It's "roughly" the same aspect ratio
// because its dimensions may be rounded down to be a multiple of |alignment|.
// The origin of the rectangle is also aligned down to a multiple of
// |alignment|. Note that |alignment| must be a power of 2.
MEDIA_EXPORT gfx::Rect CropSizeForScalingToTarget(const gfx::Size& size,
                                                  const gfx::Size& target,
                                                  size_t alignment = 1u);

// Returns the size of a rectangle whose upper left corner is at the origin (0,
// 0) and whose bottom right corner is the same as that of |rect|. This is
// useful to get the size of a buffer that contains the visible rectangle plus
// the non-visible area above and to the left of the visible rectangle.
//
// An example to illustrate: suppose the visible rectangle of a decoded frame is
// 10,10,100,100. The size of this rectangle is 90x90. However, we need to
// create a texture of size 100x100 because the client will want to sample from
// the texture starting with uv coordinates corresponding to 10,10.
MEDIA_EXPORT gfx::Size GetRectSizeFromOrigin(const gfx::Rect& rect);

// Returns |size| with only one of its dimensions increased such that the result
// matches the aspect ratio of |target|.  This is different from
// ScaleSizeToEncompassTarget() in two ways: 1) The goal is to match the aspect
// ratio of |target| rather than that of |size|.  2) Only one of the dimensions
// of |size| may change, and it may only be increased (padded).  If either
// |size| or |target| is empty, the result will be empty.
MEDIA_EXPORT gfx::Size PadToMatchAspectRatio(const gfx::Size& size,
                                             const gfx::Size& target);

// A helper function to map GpuMemoryBuffer-based VideoFrame. This function
// maps the given GpuMemoryBuffer of |frame| as-is without converting pixel
// format, unless the video frame is backed by DXGI GMB.
// The returned VideoFrame owns the |frame|.
// If the underlying buffer is DXGI, then it will be copied to shared memory
// in GPU process.
MEDIA_EXPORT scoped_refptr<VideoFrame> ConvertToMemoryMappedFrame(
    scoped_refptr<VideoFrame> frame);

// A helper function to map GpuMemoryBuffer-based VideoFrame. This function
// maps the given GpuMemoryBuffer of |frame| as-is without converting pixel
// format, unless the video frame is backed by DXGI GMB.
// The returned VideoFrame owns the |frame|.
// If the underlying buffer is DXGI, then it will be copied to shared memory
// in GPU process.
// If the GPU process is involved, the callback will be called in the
// GpuMemoryThread. Otherwise it will be involved immediately in the current
// sequence.
MEDIA_EXPORT void ConvertToMemoryMappedFrameAsync(
    scoped_refptr<VideoFrame> frame,
    base::OnceCallback<void(scoped_refptr<VideoFrame>)> result_cb);

// This function synchronously reads pixel data from textures associated with
// |txt_frame| and creates a new CPU memory backed frame. It's needed because
// existing video encoders can't handle texture backed frames.
//
// TODO(crbug.com/40162806): Combine this function with
// media::ConvertAndScaleFrame and put it into a new class
// media:FrameSizeAndFormatConverter.
MEDIA_EXPORT scoped_refptr<VideoFrame> ReadbackTextureBackedFrameToMemorySync(
    VideoFrame& txt_frame,
    gpu::raster::RasterInterface* ri,
    const gpu::Capabilities& caps,
    VideoFramePool* pool = nullptr);

// Synchronously reads a single plane. |src_rect| is relative to the plane,
// which may be smaller than |frame| due to subsampling.
MEDIA_EXPORT bool ReadbackTexturePlaneToMemorySync(
    VideoFrame& src_frame,
    size_t src_plane,
    gfx::Rect& src_rect,
    uint8_t* dest_pixels,
    size_t dest_stride,
    gpu::raster::RasterInterface* ri,
    const gpu::Capabilities& caps);

// Converts a frame with I420A format into I420 by dropping alpha channel.
MEDIA_EXPORT scoped_refptr<VideoFrame> WrapAsI420VideoFrame(
    scoped_refptr<VideoFrame> frame);

// Copy I420 video frame to match the required coded size and pad the region
// outside the visible rect repeatedly with the last column / row up to the
// coded size of |dst_frame|. Return false when |dst_frame| is empty or visible
// rect is empty.
// One application is content mirroring using HW encoder. As the required coded
// size for encoder is unknown before capturing, memory copy is needed when the
// coded size does not match the requirement. Padding can improve the encoding
// efficiency in this case, as the encoder will encode the whole coded region.
// Performance-wise, this function could be expensive as it does memory copy of
// the whole visible rect.
// Note:
// 1. |src_frame| and |dst_frame| should have same size of visible rect.
// 2. The visible rect's origin of |dst_frame| should be (0,0).
// 3. |dst_frame|'s coded size (both width and height) should be larger than or
// equal to the visible size, since the visible region in both frames should be
// identical.
[[nodiscard]] MEDIA_EXPORT bool I420CopyWithPadding(const VideoFrame& src_frame,
                                                    VideoFrame* dst_frame);

// Converts kRGBA_8888_SkColorType and kBGRA_8888_SkColorType to the appropriate
// ARGB, XRGB, ABGR, or XBGR format.
MEDIA_EXPORT VideoPixelFormat
VideoPixelFormatFromSkColorType(SkColorType sk_color_type, bool is_opaque);

// Get SkColor suitable type for various formats and planes.
MEDIA_EXPORT SkColorType SkColorTypeForPlane(VideoPixelFormat format,
                                             size_t plane);

// Backs a VideoFrame with a SkImage. The created frame takes a ref on the
// provided SkImage to make this operation zero copy. Only works with CPU
// backed images.
MEDIA_EXPORT scoped_refptr<VideoFrame> CreateFromSkImage(
    sk_sp<SkImage> sk_image,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    bool force_opaque = false);

// Utility to convert a media pixel format to SkYUVAInfo.
MEDIA_EXPORT std::tuple<SkYUVAInfo::PlaneConfig, SkYUVAInfo::Subsampling>
VideoPixelFormatToSkiaValues(VideoPixelFormat video_format);

// Returns the libyuv RGB conversion matrix for a given skia YUV color space.
// If `output_argb_matrix` is true a ARGB matrix will be provided, if false a
// ABGR matrix will be provided.
//
// NOTE: When using the ABGR matrix, you must also swap the V,U parameters to
// whichever libyuv function you're using.
MEDIA_EXPORT const libyuv::YuvConstants* GetYuvContantsForColorSpace(
    SkYUVColorSpace cs,
    bool output_argb_matrix);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_UTIL_H_
