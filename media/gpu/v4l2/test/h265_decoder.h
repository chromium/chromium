
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_H265_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_H265_DECODER_H_

#include "media/gpu/v4l2/test/video_decoder.h"

namespace media {
namespace v4l2_test {

class H265Decoder : public VideoDecoder {
 public:
  H265Decoder(const H265Decoder&) = delete;
  H265Decoder& operator=(const H265Decoder&) = delete;
  ~H265Decoder() override;

  // Creates a H265Decoder after verifying that the bitstream is HEVC
  // and the underlying implementation supports HEVC slice decoding.
  static std::unique_ptr<H265Decoder> Create(
      const base::MemoryMappedFile& stream);

  // Parses next frame from the input and decodes the frame. This method will
  // place the Y, U, and V values into the respective vectors and update the
  // size with the display area size of the decoded frame.
  VideoDecoder::Result DecodeNextFrame(const int frame_number,
                                       std::vector<uint8_t>& y_plane,
                                       std::vector<uint8_t>& u_plane,
                                       std::vector<uint8_t>& v_plane,
                                       gfx::Size& size) override;

 private:
  H265Decoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
              gfx::Size display_resolution,
              const base::MemoryMappedFile& data_stream);
};
}  // namespace v4l2_test
}  // namespace media
#endif  // MEDIA_GPU_V4L2_TEST_H265_DECODER_H_
