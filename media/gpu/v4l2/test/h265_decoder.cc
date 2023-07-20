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

// Gets bit depth info from SPS
bool ParseBitDepth(const H265SPS& sps, uint8_t& bit_depth) {
  // Spec 7.4.3.2.1
  // See spec at http://www.itu.int/rec/T-REC-H.265
  if (sps.bit_depth_y != sps.bit_depth_c) {
    LOG(ERROR) << "Different bit depths among planes is not supported";
    return false;
  }
  bit_depth = base::checked_cast<uint8_t>(sps.bit_depth_y);
  return true;
}

// Checks bit depth is supported with the given HEVC profile
bool IsValidBitDepth(uint8_t bit_depth, VideoCodecProfile profile) {
  switch (profile) {
    // Spec A.3.2
    case HEVCPROFILE_MAIN:
      return bit_depth == 8u;
    // Spec A.3.3
    case HEVCPROFILE_MAIN10:
      return bit_depth == 8u || bit_depth == 10u;
    // Spec A.3.4
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return bit_depth == 8u;
    // Spec A.3.5
    case HEVCPROFILE_REXT:
      return bit_depth == 8u || bit_depth == 10u || bit_depth == 12u ||
             bit_depth == 14u || bit_depth == 16u;
    // Spec A.3.6
    case HEVCPROFILE_HIGH_THROUGHPUT:
      return bit_depth == 8u || bit_depth == 10u || bit_depth == 14u ||
             bit_depth == 16u;
    // Spec G.11.1.1
    case HEVCPROFILE_MULTIVIEW_MAIN:
      return bit_depth == 8u;
    // Spec H.11.1.1
    case HEVCPROFILE_SCALABLE_MAIN:
      return bit_depth == 8u || bit_depth == 10u;
    // Spec I.11.1.1
    case HEVCPROFILE_3D_MAIN:
      return bit_depth == 8u;
    // Spec A.3.7
    case HEVCPROFILE_SCREEN_EXTENDED:
      return bit_depth == 8u || bit_depth == 10u;
    // Spec H.11.1.2
    case HEVCPROFILE_SCALABLE_REXT:
      return bit_depth == 8u || bit_depth == 12u || bit_depth == 16u;
    // Spec A.3.8
    case HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED:
      return bit_depth == 8u || bit_depth == 10u || bit_depth == 14u;
    default:
      LOG(ERROR) << "Invalid profile specified for H265";
      return false;
  }
}
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

bool H265Decoder::OutputAllRemainingPics() {
  NOTIMPLEMENTED();
  return false;
}

bool H265Decoder::Flush() {
  VLOGF(4) << "Decoder flush";

  if (!OutputAllRemainingPics()) {
    return false;
  }

  dpb_.Clear();
  prev_tid0_pic_ = nullptr;

  return true;
}

bool H265Decoder::ProcessPPS(int pps_id, bool* need_new_buffers) {
  VLOGF(4) << "Processing PPS id:" << pps_id;

  const H265PPS* pps = parser_->GetPPS(pps_id);
  DCHECK(pps);

  const H265SPS* sps = parser_->GetSPS(pps->pps_seq_parameter_set_id);
  DCHECK(sps);

  if (need_new_buffers) {
    *need_new_buffers = false;
  }

  gfx::Size new_pic_size = sps->GetCodedSize();
  gfx::Rect new_visible_rect = sps->GetVisibleRect();
  if (visible_rect_ != new_visible_rect) {
    VLOGF(4) << "New visible rect: " << new_visible_rect.ToString();
    visible_rect_ = new_visible_rect;
  }

  VideoChromaSampling new_chroma_sampling = sps->GetChromaSampling();
  if (new_chroma_sampling != VideoChromaSampling::k420) {
    LOG(ERROR) << "Only YUV 4:2:0 is supported";
    return false;
  }

  // Equation 7-8
  max_pic_order_cnt_lsb_ =
      std::pow(2, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);

  VideoCodecProfile new_profile = H265Parser::ProfileIDCToVideoCodecProfile(
      sps->profile_tier_level.general_profile_idc);

  uint8_t new_bit_depth = 0;
  if (!ParseBitDepth(*sps, new_bit_depth)) {
    return false;
  }

  if (!IsValidBitDepth(new_bit_depth, new_profile)) {
    LOG(ERROR) << "Invalid bit depth=" << base::strict_cast<int>(new_bit_depth)
               << ", profile=" << GetProfileName(new_profile);
    return false;
  }

  if (pic_size_ != new_pic_size || dpb_.MaxNumPics() != sps->max_dpb_size ||
      profile_ != new_profile || bit_depth_ != new_bit_depth ||
      chroma_sampling_ != new_chroma_sampling) {
    CHECK(Flush()) << "Failed to flush the decoder.";

    LOG(INFO) << "Codec profile: " << GetProfileName(new_profile)
              << ", level(x30): " << sps->profile_tier_level.general_level_idc
              << ", DPB size: " << sps->max_dpb_size
              << ", Picture size: " << new_pic_size.ToString()
              << ", bit_depth: " << base::strict_cast<int>(new_bit_depth)
              << ", chroma_sampling_format: "
              << VideoChromaSamplingToString(new_chroma_sampling);
    profile_ = new_profile;
    bit_depth_ = new_bit_depth;
    pic_size_ = new_pic_size;
    chroma_sampling_ = new_chroma_sampling;
    dpb_.SetMaxNumPics(sps->max_dpb_size);
    if (need_new_buffers) {
      *need_new_buffers = true;
    }
  }

  return true;
}

bool H265Decoder::PreprocessCurrentSlice() {
  NOTIMPLEMENTED();
  return false;
}

bool H265Decoder::ProcessCurrentSlice() {
  NOTIMPLEMENTED();
  return false;
}

void H265Decoder::CalcPicOutputFlags(const H265SliceHeader* slice_hdr) {
  NOTIMPLEMENTED();
}

void H265Decoder::CalcPictureOrderCount(const H265PPS* pps,
                                        const H265SliceHeader* slice_hdr) {
  NOTIMPLEMENTED();
}

bool H265Decoder::CalcRefPicPocs(const H265SPS* sps,
                                 const H265PPS* pps,
                                 const H265SliceHeader* slice_hdr) {
  NOTIMPLEMENTED();
  return false;
}

bool H265Decoder::BuildRefPicLists(const H265SPS* sps,
                                   const H265PPS* pps,
                                   const H265SliceHeader* slice_hdr) {
  NOTIMPLEMENTED();
  return false;
}

bool H265Decoder::PerformDpbOperations(const H265SPS* sps) {
  NOTIMPLEMENTED();
  return false;
}

bool H265Decoder::StartNewFrame(const H265SliceHeader* slice_hdr) {
  CHECK(curr_pic_.get());
  DCHECK(slice_hdr);

  curr_pps_id_ = slice_hdr->slice_pic_parameter_set_id;
  const H265PPS* pps = parser_->GetPPS(curr_pps_id_);
  DCHECK(pps);

  curr_sps_id_ = pps->pps_seq_parameter_set_id;
  const H265SPS* sps = parser_->GetSPS(curr_sps_id_);
  DCHECK(sps);

  // If this is from a retry for submitting frame meta data,
  // we should not redo all of these calculations.
  if (!curr_pic_->processed_) {
    // Copy slice/pps variables we need to the picture.
    curr_pic_->nal_unit_type_ = curr_nalu_->nal_unit_type;
    curr_pic_->irap_pic_ = slice_hdr->irap_pic;

    // TODO(b/261127809): Set the color space for the picture.

    CalcPicOutputFlags(slice_hdr);
    CalcPictureOrderCount(pps, slice_hdr);
    {
      const bool success = CalcRefPicPocs(sps, pps, slice_hdr);
      CHECK(success) << "CalcRefPicPocs function failed.";
    }
    {
      const bool success = BuildRefPicLists(sps, pps, slice_hdr);
      CHECK(success) << "BuildRefPicLists function failed.";
    }
    {
      const bool success = PerformDpbOperations(sps);
      CHECK(success) << "PerformDpbOperations function failed.";
    }
    curr_pic_->processed_ = true;
  }

  // TODO(b/261127809): Implement submit frame meta data
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
