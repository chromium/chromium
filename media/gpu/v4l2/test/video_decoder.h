// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_VIDEO_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_VIDEO_DECODER_H_

#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"

#include "media/filters/ivf_parser.h"

namespace media {

class IvfParser;

namespace v4l2_test {

// VideoDecoder decodes encoded IVF streams using v4l2 ioctl calls.
class VideoDecoder {
 public:
  // Result of decoding the current frame.
  enum Result {
    kOk,
    kError,
    kEOStream,
  };

  VideoDecoder(std::unique_ptr<IvfParser> ivf_parser,
               std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
               std::unique_ptr<V4L2Queue> OUTPUT_queue,
               std::unique_ptr<V4L2Queue> CAPTURE_queue);

  virtual ~VideoDecoder();

  VideoDecoder(const VideoDecoder&) = delete;
  VideoDecoder& operator=(const VideoDecoder&) = delete;

  // Initializes setup needed for decoding.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/dev-stateless-decoder.html#initialization
  void Initialize();

  virtual Result DecodeNextFrame(std::vector<char>& y_plane,
                                 std::vector<char>& u_plane,
                                 std::vector<char>& v_plane,
                                 gfx::Size& size,
                                 const int frame_number) = 0;

  // Returns whether the last decoded frame was visible.
  bool LastDecodedFrameVisible() const { return last_decoded_frame_visible_; }

 protected:
  // Parser for the IVF stream to decode.
  const std::unique_ptr<IvfParser> ivf_parser_;

  // Wrapper for V4L2 ioctl requests.
  const std::unique_ptr<V4L2IoctlShim> v4l2_ioctl_;

  // OUTPUT_queue needed for compressed (encoded) input.
  std::unique_ptr<V4L2Queue> OUTPUT_queue_;

  // CAPTURE_queue needed for uncompressed (decoded) output.
  std::unique_ptr<V4L2Queue> CAPTURE_queue_;

  // Whether the last decoded frame was visible.
  bool last_decoded_frame_visible_ = false;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_VIDEO_DECODER_H_
