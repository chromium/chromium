// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_VIDEO_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_VIDEO_DECODER_H_

#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"

namespace media {
namespace v4l2_test {

// For stateless API, fourcc |VP9F| is needed instead of |VP90| for VP9 codec.
// Fourcc |AV1F| is needed instead of |AV10| for AV1 codec.
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/pixfmt-compressed.html
// Converts fourcc |VP90| or |AV01| from file header to fourcc |VP9F| or |AV1F|,
// which is a format supported on driver.
uint32_t FileFourccToDriverFourcc(uint32_t header_fourcc);

// VideoDecoder decodes encoded video streams using v4l2 ioctl calls.
class VideoDecoder {
 public:
  // Result of decoding the current frame.
  enum Result {
    kOk,
    kError,
    kEOStream,
  };

  VideoDecoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
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

  // Handles dynamic resolution change with new resolution parsed from frame
  // header.
  VideoDecoder::Result HandleDynamicResolutionChange(
      const gfx::Size& new_resolution);

  // Returns whether the last decoded frame was visible.
  bool LastDecodedFrameVisible() const { return last_decoded_frame_visible_; }

  // Return whether there is a dynamic resolytion change.
  bool IsResolutionChanged() const { return is_resolution_changed_; }

 protected:
  // Helper method for converting NV12 frames to I420.
  static void ConvertNV12ToYUV(std::vector<char>& dest_y,
                               std::vector<char>& dest_u,
                               std::vector<char>& dest_v,
                               const gfx::Size& dest_size,
                               const char* src,
                               const gfx::Size& src_size);

  // Helper method for converting MM21 frames to I420.
  static void ConvertMM21ToYUV(std::vector<char>& dest_y,
                               std::vector<char>& dest_u,
                               std::vector<char>& dest_v,
                               const gfx::Size& dest_size,
                               char* src_y,
                               char* src_uv,
                               const gfx::Size& src_size);

  // Wrapper for V4L2 ioctl requests.
  const std::unique_ptr<V4L2IoctlShim> v4l2_ioctl_;

  // OUTPUT_queue needed for compressed (encoded) input.
  std::unique_ptr<V4L2Queue> OUTPUT_queue_;

  // CAPTURE_queue needed for uncompressed (decoded) output.
  std::unique_ptr<V4L2Queue> CAPTURE_queue_;

  // Whether the last decoded frame was visible.
  bool last_decoded_frame_visible_ = false;

  // Whether there is a dynamic support change.
  bool is_resolution_changed_ = false;

  // Number of buffers in CAPTURE queue varied by different codecs.
  uint32_t number_of_buffers_in_capture_queue_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_VIDEO_DECODER_H_
