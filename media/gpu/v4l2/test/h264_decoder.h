// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_GPU_V4L2_TEST_H264_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_H264_DECODER_H_

#include "base/files/memory_mapped_file.h"
#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"
#include "media/gpu/v4l2/test/video_decoder.h"
#include "media/video/h264_parser.h"

namespace media {
namespace v4l2_test {
class H264Decoder : public VideoDecoder {
 public:
  H264Decoder(const H264Decoder&) = delete;
  H264Decoder& operator=(const H264Decoder&) = delete;
  ~H264Decoder() override;

  // Creates a H264Decoder after verifying that the bitstream is h.264
  // and the underlying implementation supports H.264 slice decoding.
  static std::unique_ptr<H264Decoder> Create(
      const base::MemoryMappedFile& stream);

  // Parses next frame from IVF stream and decodes the frame.  This method will
  // place the Y, U, and V values into the respective vectors and update the
  // size with the display area size of the decoded frame.
  VideoDecoder::Result DecodeNextFrame(std::vector<char>& y_plane,
                                       std::vector<char>& u_plane,
                                       std::vector<char>& v_plane,
                                       gfx::Size& size,
                                       const int frame_number) override;

 private:
  H264Decoder(std::unique_ptr<H264Parser> parser,
              std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
              std::unique_ptr<V4L2Queue> OUTPUT_queue,
              std::unique_ptr<V4L2Queue> CAPTURE_queue);

  // Processes NALU's until reaching the end of the current frame.  This method
  // will send Ext Ctrls via IOCTL calls to indicate the start of a frame.
  H264Parser::Result ProcessNextFrame(
      std::unique_ptr<H264SliceHeader>* resulting_slice_header);

  // Sends IOCTL call to device with the frame's SPS, PPS, and Scaling Matrix
  // data which indicates the beginning of a new frame.
  VideoDecoder::Result StartNewFrame(int sps_id, int pps_id);

  // Transmits each H264 Slice associated with the current frame to the
  // device.  Additionally sends Decode Parameters and Decode Mode
  // via IOCTL Ext Ctrls.
  VideoDecoder::Result SubmitSlice(H264SliceHeader curr_slice, int frame_num);

  const std::unique_ptr<H264Parser> parser_;

  std::unique_ptr<H264NALU> pending_nalu_;
  std::unique_ptr<H264SliceHeader> pending_slice_header_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_H264_DECODER_H_
