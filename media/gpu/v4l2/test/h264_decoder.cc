// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/h264_decoder.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/video/h264_parser.h"

namespace media {

namespace v4l2_test {

constexpr uint32_t kNumberOfBuffersInCaptureQueue = 10;

static_assert(kNumberOfBuffersInCaptureQueue <= 16,
              "Too many CAPTURE buffers are used. The number of CAPTURE "
              "buffers is currently assumed to be no larger than 16.");

// static
std::unique_ptr<H264Decoder> H264Decoder::Create(
    const base::MemoryMappedFile& stream) {
  H264Parser parser;
  parser.SetStream(stream.data(), stream.length());

  // Advance through NALUs until the first SPS.  The start of the decodable
  // data in an h.264 bistreams starts with an SPS.
  while (true) {
    H264NALU nalu;
    H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res != H264Parser::kOk) {
      LOG(ERROR) << "Unable to find SPS in stream";
      return nullptr;
    }

    if (nalu.nal_unit_type == H264NALU::kSPS)
      break;
  }

  int id;
  H264Parser::Result res = parser.ParseSPS(&id);
  CHECK(res == H264Parser::kOk);

  const H264SPS* sps = parser.GetSPS(id);
  CHECK(sps);

  absl::optional<gfx::Size> coded_size = sps->GetCodedSize();
  CHECK(coded_size);
  LOG(INFO) << "h.264 coded size : " << coded_size->ToString();

  return nullptr;
}

H264Decoder::H264Decoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                         std::unique_ptr<V4L2Queue> OUTPUT_queue,
                         std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl),
                                 std::move(OUTPUT_queue),
                                 std::move(CAPTURE_queue)) {}

H264Decoder::~H264Decoder() = default;

VideoDecoder::Result H264Decoder::DecodeNextFrame(std::vector<char>& y_plane,
                                                  std::vector<char>& u_plane,
                                                  std::vector<char>& v_plane,
                                                  gfx::Size& size,
                                                  const int frame_number) {
  return VideoDecoder::kError;
}

}  // namespace v4l2_test
}  // namespace media
