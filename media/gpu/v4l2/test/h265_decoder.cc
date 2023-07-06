// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/h265_decoder.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "media/gpu/v4l2/test/upstream_pix_fmt.h"
#include "media/video/h265_parser.h"

namespace media {
namespace v4l2_test {

namespace {
constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_HEVC_SLICE;
}

H265Decoder::H265Decoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                         gfx::Size display_resolution,
                         const base::MemoryMappedFile& data_stream)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl), display_resolution) {}

H265Decoder::~H265Decoder() = default;

// static
std::unique_ptr<H265Decoder> H265Decoder::Create(
    const base::MemoryMappedFile& stream) {
  auto parser = std::make_unique<H265Parser>();
  parser->SetStream(stream.data(), stream.length());

  // Advance through NALUs until the first SPS.  The start of the decodable
  // data in an H.265 bistreams starts with an SPS.
  while (true) {
    H265NALU nalu;
    H265Parser::Result res = parser->AdvanceToNextNALU(&nalu);
    if (res != H265Parser::kOk) {
      LOG(ERROR) << "Unable to find SPS in stream";
      return nullptr;
    }

    if (nalu.nal_unit_type == H265NALU::SPS_NUT) {
      break;
    }
  }

  int sps_id;
  H265Parser::Result res = parser->ParseSPS(&sps_id);
  CHECK(res == H265Parser::kOk);

  const H265SPS* sps = parser->GetSPS(sps_id);
  CHECK(sps);

  absl::optional<gfx::Size> coded_size = sps->GetCodedSize();
  CHECK(coded_size);
  LOG(INFO) << "H.265 coded size : " << coded_size->ToString();

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>(kDriverCodecFourcc);

  if (!v4l2_ioctl->VerifyCapabilities(kDriverCodecFourcc)) {
    LOG(ERROR) << "Device doesn't support "
               << media::FourccToString(kDriverCodecFourcc) << ".";
    return nullptr;
  }

  return base::WrapUnique(
      new H265Decoder(std::move(v4l2_ioctl), coded_size.value(), stream));
}

VideoDecoder::Result H265Decoder::DecodeNextFrame(const int frame_number,
                                                  std::vector<uint8_t>& y_plane,
                                                  std::vector<uint8_t>& u_plane,
                                                  std::vector<uint8_t>& v_plane,
                                                  gfx::Size& size) {
  NOTIMPLEMENTED();
  return VideoDecoder::kOk;
}
}  // namespace v4l2_test
}  // namespace media
