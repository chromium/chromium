// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_VIDEO_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_VIDEO_DECODER_H_

#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"

namespace media {
namespace v4l2_test {

constexpr uint32_t kNumberOfBuffersInOutputQueue = 1;
static_assert(kNumberOfBuffersInOutputQueue == 1,
              "Too many buffers in OUTPUT queue. It is currently designed to "
              "support only 1 request at a time.");

// For stateless API, fourcc |VP9F| is needed instead of |VP90| for VP9 codec.
// Fourcc |AV1F| is needed instead of |AV10| for AV1 codec.
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/pixfmt-compressed.html
// Converts fourcc |VP90| or |AV01| from file header to fourcc |VP9F| or |AV1F|,
// which is a format supported on driver.
uint32_t FileFourccToDriverFourcc(uint32_t header_fourcc);

// VideoDecoder decodes encoded video streams using v4l2 ioctl calls.
// To implement a decoder, implement the following:
// 1. A factory function, such as:
//   std::unique_ptr<VideoDecoder> Create(const base::MemoryMappedFile& stream)
// 2. DecodeNextFrame
class VideoDecoder {
 public:
  // Result of decoding the current frame.
  enum Result {
    kOk,
    kError,
    kEOStream,
  };

  enum BitDepth { Depth8, Depth16 };

  VideoDecoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
               gfx::Size display_resolution);

  virtual ~VideoDecoder();

  VideoDecoder(const VideoDecoder&) = delete;
  VideoDecoder& operator=(const VideoDecoder&) = delete;

  // Initializes setup needed for decoding.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/dev-stateless-decoder.html#initialization
  void CreateOUTPUTQueue(uint32_t compressed_fourcc);
  void CreateCAPTUREQueue(uint32_t num_buffers);

  // Decoders implement this. The function writes the next displayed picture
  // into the output plane buffers |y_plane|, |u_plane|, and |v_plane|. |size|
  // is the visible picture size.
  virtual Result DecodeNextFrame(const int frame_number,
                                 std::vector<uint8_t>& y_plane,
                                 std::vector<uint8_t>& u_plane,
                                 std::vector<uint8_t>& v_plane,
                                 gfx::Size& size,
                                 BitDepth& bit_depth) = 0;

  // Handles dynamic resolution change with new resolution parsed from frame
  // header.
  void HandleDynamicResolutionChange(const gfx::Size& new_resolution);

  // Returns whether the last decoded frame was visible.
  bool LastDecodedFrameVisible() const { return last_decoded_frame_visible_; }

  // Converts raw YUV of decoded frame data to PNG.
  static std::vector<uint8_t> ConvertYUVToPNG(uint8_t* y_plane,
                                              uint8_t* u_plane,
                                              uint8_t* v_plane,
                                              const gfx::Size& size,
                                              BitDepth bit_depth);

 protected:
  void NegotiateCAPTUREFormat();

  // Helper method for converting frames to YUV. Returns the bit depth of
  // the converted frame.
  static BitDepth ConvertToYUV(std::vector<uint8_t>& dest_y,
                               std::vector<uint8_t>& dest_u,
                               std::vector<uint8_t>& dest_v,
                               const gfx::Size& dest_size,
                               const MmappedBuffer::MmappedPlanes& planes,
                               const gfx::Size& src_size,
                               uint32_t fourcc);

  // Wrapper for V4L2 ioctl requests.
  const std::unique_ptr<V4L2IoctlShim> v4l2_ioctl_;

  // OUTPUT_queue needed for compressed (encoded) input.
  std::unique_ptr<V4L2Queue> OUTPUT_queue_;

  // CAPTURE_queue needed for uncompressed (decoded) output.
  std::unique_ptr<V4L2Queue> CAPTURE_queue_;

  // Whether the last decoded frame was visible.
  bool last_decoded_frame_visible_ = false;

  // resolution from the bitstream header
  gfx::Size display_resolution_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_VIDEO_DECODER_H_
