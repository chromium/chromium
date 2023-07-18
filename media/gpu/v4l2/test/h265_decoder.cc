// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/h265_decoder.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "media/gpu/macros.h"
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
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl), display_resolution),
      data_stream_(data_stream) {}

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
  const H265Parser::Result parse_result = parser->ParseSPS(&sps_id);
  CHECK_EQ(parse_result, H265Parser::kOk);

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

bool H265Decoder::ProcessPPS(int pps_id, bool* need_new_buffers) {
  NOTIMPLEMENTED();
  return false;
}

bool H265Decoder::PreprocessCurrentSlice() {
  NOTIMPLEMENTED();
  return false;
}

bool H265Decoder::ProcessCurrentSlice() {
  NOTIMPLEMENTED();
  return false;
}

bool H265Decoder::StartNewFrame(const H265SliceHeader* slice_hdr) {
  NOTIMPLEMENTED();
  return false;
}

bool H265Decoder::FinishPrevFrameIfPresent() {
  NOTIMPLEMENTED();
  return false;
}

H265Decoder::DecodeResult H265Decoder::Decode() {
  DCHECK(state_ != kError) << "Decoder in error state";

  while (true) {
    if (!curr_nalu_) {
      curr_nalu_ = std::make_unique<H265NALU>();

      const H265Parser::Result parse_result =
          parser_->AdvanceToNextNALU(curr_nalu_.get());
      if (parse_result == H265Parser::kEOStream) {
        curr_nalu_.reset();

        const bool success = FinishPrevFrameIfPresent();
        CHECK(success) << "Failed to finish processing the previous frame.";

        is_stream_over_ = true;
        return kRanOutOfStreamData;
      }

      CHECK_EQ(parse_result, H265Parser::kOk);
      VLOGF(4) << "New NALU: " << static_cast<int>(curr_nalu_->nal_unit_type);
    }

    // 8.1.2 We only want nuh_layer_id of zero.
    if (curr_nalu_->nuh_layer_id) {
      VLOGF(4) << "Skipping NALU with nuh_layer_id="
               << curr_nalu_->nuh_layer_id;
      curr_nalu_.reset();
      continue;
    }

    switch (curr_nalu_->nal_unit_type) {
      case H265NALU::BLA_W_LP:  // fallthrough
      case H265NALU::BLA_W_RADL:
      case H265NALU::BLA_N_LP:
      case H265NALU::IDR_W_RADL:
      case H265NALU::IDR_N_LP:
      case H265NALU::TRAIL_N:
      case H265NALU::TRAIL_R:
      case H265NALU::TSA_N:
      case H265NALU::TSA_R:
      case H265NALU::STSA_N:
      case H265NALU::STSA_R:
      case H265NALU::RADL_N:
      case H265NALU::RADL_R:
      case H265NALU::RASL_N:
      case H265NALU::RASL_R:
      case H265NALU::CRA_NUT: {
        if (!curr_slice_hdr_) {
          curr_slice_hdr_ = std::make_unique<H265SliceHeader>();

          const H265Parser::Result parse_result = parser_->ParseSliceHeader(
              *curr_nalu_, curr_slice_hdr_.get(), last_slice_hdr_.get());
          if (parse_result == H265Parser::kMissingParameterSet) {
            // We may still be able to recover if we skip until we find the
            // SPS/PPS.
            curr_slice_hdr_.reset();
            last_slice_hdr_.reset();
            break;
          }

          CHECK_EQ(parse_result, H265Parser::kOk);
          if (!curr_slice_hdr_->irap_pic && state_ == kAfterReset) {
            // We can't resume from a non-IRAP picture.
            curr_slice_hdr_.reset();
            last_slice_hdr_.reset();
            break;
          }

          state_ = kTryPreprocessCurrentSlice;
          if (curr_slice_hdr_->irap_pic) {
            bool need_new_buffers = false;

            const bool success = ProcessPPS(
                curr_slice_hdr_->slice_pic_parameter_set_id, &need_new_buffers);
            CHECK(success) << "Failed to process PPS.";

            if (need_new_buffers) {
              curr_pic_ = nullptr;
              return kConfigChange;
            }
          }
        }

        if (state_ == kTryPreprocessCurrentSlice) {
          const bool success = PreprocessCurrentSlice();
          CHECK(success) << "Failed to pre-process current slice.";

          state_ = kEnsurePicture;
        }

        if (state_ == kEnsurePicture) {
          if (curr_pic_) {
            // |curr_pic_| already exists, so skip to ProcessCurrentSlice().
            state_ = kTryCurrentSlice;
          } else {
            curr_pic_ = new H265Picture();
            CHECK(curr_pic_) << "Ran out of surfaces.";

            curr_pic_->first_picture_ = first_picture_;
            first_picture_ = false;
            state_ = kTryNewFrame;
          }
        }

        if (state_ == kTryNewFrame) {
          const bool success = StartNewFrame(curr_slice_hdr_.get());
          CHECK(success) << "Failed to start processing a new frame.";

          state_ = kTryCurrentSlice;
        }

        DCHECK_EQ(state_, kTryCurrentSlice);
        const bool success = ProcessCurrentSlice();
        CHECK(success) << "Failed to process current slice.";

        state_ = kDecoding;
        last_slice_hdr_ = std::move(curr_slice_hdr_);
        curr_slice_hdr_.reset();
        break;
      }
      case H265NALU::SPS_NUT: {
        const bool success = FinishPrevFrameIfPresent();
        CHECK(success) << "Failed to finish processing the previous frame.";

        int sps_id;

        const H265Parser::Result parse_result = parser_->ParseSPS(&sps_id);
        CHECK_EQ(parse_result, H265Parser::kOk)
            << "Parser Failed to parse SPS.";

        break;
      }
      case H265NALU::PPS_NUT: {
        const bool success = FinishPrevFrameIfPresent();
        CHECK(success) << "Failed to finish processing the previous frame.";

        int pps_id;

        const H265Parser::Result parse_result =
            parser_->ParsePPS(*curr_nalu_, &pps_id);
        CHECK_EQ(parse_result, H265Parser::kOk)
            << "Parser Failed to parse PPS.";

        // TODO(b/261127809): add code to set |curr_pps_id_| in StartNewFrame()
        if (curr_pps_id_ == -1) {
          bool need_new_buffers = false;

          const bool success_process_pps =
              ProcessPPS(pps_id, &need_new_buffers);
          CHECK(success_process_pps) << "Failed to process PPS.";

          if (need_new_buffers) {
            curr_nalu_.reset();
            return kConfigChange;
          }
        }

        break;
      }
      case H265NALU::EOS_NUT:
        first_picture_ = true;
        [[fallthrough]];
      case H265NALU::EOB_NUT:  // fallthrough
      case H265NALU::AUD_NUT:
      case H265NALU::RSV_NVCL41:
      case H265NALU::RSV_NVCL42:
      case H265NALU::RSV_NVCL43:
      case H265NALU::RSV_NVCL44:
      case H265NALU::UNSPEC48:
      case H265NALU::UNSPEC49:
      case H265NALU::UNSPEC50:
      case H265NALU::UNSPEC51:
      case H265NALU::UNSPEC52:
      case H265NALU::UNSPEC53:
      case H265NALU::UNSPEC54:
      case H265NALU::UNSPEC55: {
        const bool success = FinishPrevFrameIfPresent();
        CHECK(success) << "Failed to finish processing the previous frame.";
        break;
      }
      default:
        VLOGF(4) << "Skipping NALU type: " << curr_nalu_->nal_unit_type;
        break;
    }

    VLOGF(4) << "Finished with current NALU: "
             << static_cast<int>(curr_nalu_->nal_unit_type);
    curr_nalu_.reset();
  }
}

VideoDecoder::Result H265Decoder::DecodeNextFrame(const int frame_number,
                                                  std::vector<uint8_t>& y_plane,
                                                  std::vector<uint8_t>& u_plane,
                                                  std::vector<uint8_t>& v_plane,
                                                  gfx::Size& size) {
  if (!parser_) {
    parser_ = std::make_unique<H265Parser>();
    parser_->SetStream(data_stream_.data(), data_stream_.length());
  }

  // TODO(b/261127809): add a condition to check frames are ready for processing
  while (!is_stream_over_) {
    Decode();
  }

  NOTIMPLEMENTED();
  return VideoDecoder::kOk;
}
}  // namespace v4l2_test
}  // namespace media
