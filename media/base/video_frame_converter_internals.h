// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_CONVERTER_INTERNALS_H_
#define MEDIA_BASE_VIDEO_FRAME_CONVERTER_INTERNALS_H_

#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/color_space.h"

namespace media::internals {

MEDIA_EXPORT const libyuv::ArgbConstants* GetArgbConstantsForColorSpace(
    const gfx::ColorSpace& cs,
    bool is_abgr);

// These are all VideoFrame based versions of equivalent libyuv calls. They
// allow calling code to not have to manually coordinate which planes, strides,
// and sizes go with which method and parameter (which is not always obvious).
//
// If a libyuv method returns a status code, the method has a bool signature and
// will return false if conversion failed.
//
// If the source and destination format have alpha it will be converted.
//
// If a method doesn't support scaling it's noted in the comments.

// Scaling not supported.
MEDIA_EXPORT void CopyVisiblePlanes(const VideoFrame& src_frame,
                                    VideoFrame& dst_frame);

bool ARGBScale(const VideoFrame& src_frame,
               VideoFrame& dst_frame,
               libyuv::FilterMode filter);

bool ARGBToI420x(const VideoFrame& src_frame,
                 VideoFrame& dst_frame,
                 const libyuv::ArgbConstants* matrix);

bool ARGBToI422x(const VideoFrame& src_frame,
                 VideoFrame& dst_frame,
                 const libyuv::ArgbConstants* matrix);

bool ARGBToI444x(const VideoFrame& src_frame,
                 VideoFrame& dst_frame,
                 const libyuv::ArgbConstants* matrix);

bool ARGBToNVxx(const VideoFrame& src_frame,
                VideoFrame& dst_frame,
                const libyuv::ArgbConstants* matrix);

bool ARGBToPx10(const VideoFrame& src_frame,
                VideoFrame& dst_frame,
                const libyuv::ArgbConstants* matrix);

// Also converts between I420, I422, I444 and vice versa.
void I4xxxScale(const VideoFrame& src_frame, VideoFrame& dst_frame);

void I4xxxScale_16(const VideoFrame& src_frame, VideoFrame& dst_frame);

void Convert16To8Plane(const VideoFrame& src_frame, VideoFrame& dst_frame);
void Convert8To16Plane(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Converts a 12-bit frame in place to 10-bit.
void Shift12To10(VideoFrame& frame);

// Scaling not supported.
bool I4xxxToNVxx(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
MEDIA_EXPORT void MergeUV(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
void SplitUV(const VideoFrame& src_frame, VideoFrame& dst_frame);

bool NVxxScale(const VideoFrame& src_frame,
               VideoFrame& dst_frame,
               libyuv::FilterMode filter);

bool Px10Scale(const VideoFrame& src_frame,
               VideoFrame& dst_frame,
               libyuv::FilterMode filter);

// Scaling not supported.
void NVxxToI4xxx(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
bool NVxxToPx10(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
bool I4xxxToPx10(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
bool I4xxxPxxToPx10(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
bool Px10ToIx10(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
bool Px10ToI420(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
void Px10ToNVxx(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Converts RGBAF16 to P010LE, P210LE or P410LE. libyuv has no half float
// input, so this is not a libyuv wrapper: the pixels are color managed by Skia
// from the source color space into the RGB form of `dst_frame`'s color space,
// then converted with that color space's matrix and range at 10 bits, and
// chroma is box filtered. An invalid source color space is taken to be the RGB
// form of the destination, which leaves only the matrix, like the 8-bit RGB
// conversions. Values outside the encodable range are clamped and alpha is
// dropped. Returns false if a color space has no SkColorSpace equivalent.
//
// Scaling not supported.
MEDIA_EXPORT bool RGBAF16ToPx10(const VideoFrame& src_frame,
                                VideoFrame& dst_frame);

}  // namespace media::internals

#endif  // MEDIA_BASE_VIDEO_FRAME_CONVERTER_INTERNALS_H_
