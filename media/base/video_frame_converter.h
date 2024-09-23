// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_CONVERTER_H_
#define MEDIA_BASE_VIDEO_FRAME_CONVERTER_H_

#include "media/base/encoder_status.h"
#include "media/base/frame_buffer_pool.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"

namespace media {

// A class for converting and scaling between two VideoFrame formats. Maintains
// a scratch space, so if callers have multiples frames to convert they should
// hold onto an instance of this class for all conversions. Thread safe.
class MEDIA_EXPORT VideoFrameConverter {
 public:
  VideoFrameConverter();
  ~VideoFrameConverter();

  // Copy pixel data from `src_frame` to `dest_frame` applying scaling and pixel
  // format conversion as needed. Formats supported:
  //
  // Input formats:
  //   * PIXEL_FORMAT_XBGR
  //   * PIXEL_FORMAT_XRGB
  //   * PIXEL_FORMAT_ABGR
  //   * PIXEL_FORMAT_ARGB
  //   * PIXEL_FORMAT_I420
  //   * PIXEL_FORMAT_I420A
  //   * PIXEL_FORMAT_I444
  //   * PIXEL_FORMAT_I444A
  //   * PIXEL_FORMAT_NV12
  //   * PIXEL_FORMAT_NV12A
  //
  // Output formats:
  //   * PIXEL_FORMAT_I420
  //   * PIXEL_FORMAT_I420A
  //   * PIXEL_FORMAT_I444
  //   * PIXEL_FORMAT_I444A
  //   * PIXEL_FORMAT_NV12
  //   * PIXEL_FORMAT_NV12A
  EncoderStatus ConvertAndScale(const VideoFrame& src_frame,
                                VideoFrame& dest_frame);

  size_t get_pool_size_for_testing() const {
    return frame_pool_->get_pool_size_for_testing();
  }

 private:
  // Creates a temporary frame backed by `frame_pool_`.
  scoped_refptr<VideoFrame> CreateTempFrame(VideoPixelFormat format,
                                            const gfx::Size& coded_size,
                                            const gfx::Rect& visible_rect,
                                            const gfx::Size& natural_size);

  // Wraps an NV12x frame within an I420x frame with the Y and A planes of the
  // I420x wrapping frame pointing directly into the Y and A planes of the NV12x
  // frame. The U and V planes of the I420x wrapper point into `scratch_space_`.
  //
  // Allows for conversion of NV12 data into I420 data without copies of the
  // Y and A planes.
  //
  // Warning: VideoFrame will const_cast away the protections on `frame`, so
  // it's the callers responsibility to ensure they write only to the planes
  // they intend to.
  scoped_refptr<VideoFrame> WrapNV12xFrameInI420xFrame(const VideoFrame& frame);

  EncoderStatus ConvertAndScaleRGB(const VideoFrame* src_frame,
                                   VideoFrame& dest_frame);
  EncoderStatus ConvertAndScaleI4xxx(const VideoFrame* src_frame,
                                     VideoFrame& dest_frame);
  EncoderStatus ConvertAndScaleNV12x(const VideoFrame* src_frame,
                                     VideoFrame& dest_frame);

  // NOTE: This class is currently thread safe without locking, take care when
  // adding any shared class state.

  // Scratch space for conversions. FrameBufferPool is thread safe and will
  // helpfully auto-expire stale buffers after some time.
  scoped_refptr<FrameBufferPool> frame_pool_;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_CONVERTER_H_
