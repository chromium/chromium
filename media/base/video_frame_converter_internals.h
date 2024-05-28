// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_CONVERTER_INTERNALS_H_
#define MEDIA_BASE_VIDEO_FRAME_CONVERTER_INTERNALS_H_

#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media::internals {

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

bool ARGBScale(const VideoFrame& src_frame,
               VideoFrame& dst_frame,
               libyuv::FilterMode filter);

bool ARGBToI420x(const VideoFrame& src_frame, VideoFrame& dst_frame);

bool ARGBToI444x(const VideoFrame& src_frame, VideoFrame& dst_frame);

bool ARGBToNV12x(const VideoFrame& src_frame, VideoFrame& dst_frame);

bool ABGRToARGB(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Also converts between I420, I422, I444 and vice versa.
void I4xxxScale(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
bool I420xToNV12x(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
bool I444xToNV12x(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
void MergeUV(const VideoFrame& src_frame, VideoFrame& dst_frame);

// Scaling not supported.
void SplitUV(const VideoFrame& src_frame, VideoFrame& dst_frame);

bool NV12xScale(const VideoFrame& src_frame,
                VideoFrame& dst_frame,
                libyuv::FilterMode filter);

// Scaling not supported.
bool NV12xToI420x(const VideoFrame& src_frame, VideoFrame& dst_frame);

}  // namespace media::internals

#endif  // MEDIA_BASE_VIDEO_FRAME_CONVERTER_INTERNALS_H_
