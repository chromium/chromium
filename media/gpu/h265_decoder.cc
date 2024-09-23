// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/h265_decoder.h"

#include <algorithm>

#include "base/logging.h"
#include "base/notreached.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/video_types.h"

namespace media {

namespace {

struct POCAscCompare {
  bool operator()(const scoped_refptr<H265Picture>& a,
                  const scoped_refptr<H265Picture>& b) const {
    return a->pic_order_cnt_val_ < b->pic_order_cnt_val_;
  }
};

bool ParseBitDepth(const H265SPS& sps, uint8_t& bit_depth) {
  // Spec 7.4.3.2.1
  if (sps.bit_depth_y != sps.bit_depth_c) {
    DVLOG(1) << "Different bit depths among planes is not supported";
    return false;
  }
  bit_depth = base::checked_cast<uint8_t>(sps.bit_depth_y);
  return true;
}

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
      DVLOG(1) << "Invalid profile specified for H265";
      return false;
  }
}
}  // namespace

H265Decoder::H265Accelerator::H265Accelerator() = default;

H265Decoder::H265Accelerator::~H265Accelerator() = default;

scoped_refptr<H265Picture>
H265Decoder::H265Accelerator::CreateH265PictureSecure(uint64_t secure_handle) {
  return nullptr;
}

void H265Decoder::H265Accelerator::ProcessVPS(
    const H265VPS* vps,
    base::span<const uint8_t> vps_nalu_data) {}

void H265Decoder::H265Accelerator::ProcessSPS(
    const H265SPS* sps,
    base::span<const uint8_t> sps_nalu_data) {}

void H265Decoder::H265Accelerator::ProcessPPS(
    const H265PPS* pps,
    base::span<const uint8_t> pps_nalu_data) {}

H265Decoder::H265Accelerator::Status H265Decoder::H265Accelerator::SetStream(
    base::span<const uint8_t> stream,
    const DecryptConfig* decrypt_config) {
  return H265Decoder::H265Accelerator::Status::kNotSupported;
}

bool H265Decoder::H265Accelerator::IsAlphaLayerSupported() {
  return false;
}

H265Decoder::H265Decoder(std::unique_ptr<H265Accelerator> accelerator,
                         VideoCodecProfile profile,
                         const VideoColorSpace& container_color_space)
    : state_(kAfterReset),
      container_color_space_(container_color_space),
      profile_(profile),
      accelerator_(std::move(accelerator)) {
  DCHECK(accelerator_);
  Reset();
}

H265Decoder::~H265Decoder() = default;

#define SET_ERROR_AND_RETURN()         \
  do {                                 \
    DVLOG(1) << "Error during decode"; \
    state_ = kError;                   \
    return H265Decoder::kDecodeError;  \
  } while (0)

#define CHECK_ACCELERATOR_RESULT(func)                       \
  do {                                                       \
    H265Accelerator::Status result = (func);                 \
    switch (result) {                                        \
      case H265Accelerator::Status::kOk:                     \
        break;                                               \
      case H265Accelerator::Status::kTryAgain:               \
        DVLOG(1) << #func " needs to try again";             \
        return H265Decoder::kTryAgain;                       \
      case H265Accelerator::Status::kFail: /* fallthrough */ \
      case H265Accelerator::Status::kNotSupported:           \
        SET_ERROR_AND_RETURN();                              \
    }                                                        \
  } while (0)

void H265Decoder::SetStream(int32_t id, const DecoderBuffer& decoder_buffer) {
  const uint8_t* ptr = decoder_buffer.data();
  const size_t size = decoder_buffer.size();
  const DecryptConfig* decrypt_config = decoder_buffer.decrypt_config();

  DCHECK(ptr);
  DCHECK(size);
  DVLOG(4) << "New input stream id: " << id << " at: " << (void*)ptr
           << " size: " << size;
  stream_id_ = id;
  current_stream_ = ptr;
  current_stream_size_ = size;
  current_stream_has_been_changed_ = true;
  if (decrypt_config) {
    parser_.SetEncryptedStream(ptr, size, decrypt_config->subsamples());
    current_decrypt_config_ = decrypt_config->Clone();
  } else {
    parser_.SetStream(ptr, size);
    current_decrypt_config_ = nullptr;
  }
  if (decoder_buffer.has_side_data() &&
      decoder_buffer.side_data()->secure_handle) {
    secure_handle_ = decoder_buffer.side_data()->secure_handle;
  } else {
    secure_handle_ = 0;
  }
}

void H265Decoder::Reset() {
  first_picture_ = true;
  no_rasl_output_flag_ = true;

  curr_pic_ = nullptr;
  curr_nalu_ = nullptr;
  curr_slice_hdr_ = nullptr;
  last_slice_hdr_ = nullptr;
  curr_sps_id_ = -1;
  curr_pps_id_ = -1;
  aux_alpha_layer_id_ = 0;

  prev_tid0_pic_ = nullptr;
  ref_pic_list_.clear();
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();
  ref_pic_set_lt_curr_.clear();
  ref_pic_set_st_curr_after_.clear();
  ref_pic_set_st_curr_before_.clear();

  dpb_.Clear();
  parser_.Reset();
  accelerator_->Reset();

  secure_handle_ = 0;

  state_ = kAfterReset;
}

H265Decoder::DecodeResult H265Decoder::Decode() {
  if (state_ == kError) {
    DVLOG(1) << "Decoder in error state";
    return kDecodeError;
  }

  if (current_stream_has_been_changed_) {
    // Calling H265Accelerator::SetStream() here instead of when the stream is
    // originally set in case the accelerator needs to return kTryAgain.
    H265Accelerator::Status result = accelerator_->SetStream(
        base::span<const uint8_t>(current_stream_.get(), current_stream_size_),
        current_decrypt_config_.get());
    switch (result) {
      case H265Accelerator::Status::kOk:  // fallthrough
      case H265Accelerator::Status::kNotSupported:
        // kNotSupported means the accelerator can't handle this stream,
        // so everything will be done through the parser.
        break;
      case H265Accelerator::Status::kTryAgain:
        DVLOG(1) << "SetStream() needs to try again";
        return H265Decoder::kTryAgain;
      case H265Accelerator::Status::kFail:
        SET_ERROR_AND_RETURN();
    }

    // Reset the flag so that this is only called again next time SetStream()
    // is called.
    current_stream_has_been_changed_ = false;
  }

  while (true) {
    H265Parser::Result par_res;

    if (!curr_nalu_) {
      curr_nalu_ = std::make_unique<H265NALU>();
      par_res = parser_.AdvanceToNextNALU(curr_nalu_.get());
      if (par_res == H265Parser::kEOStream) {
        curr_nalu_.reset();
        // We receive one frame per buffer, so we can output the frame now.
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        return kRanOutOfStreamData;
      }
      if (par_res != H265Parser::kOk) {
        curr_nalu_.reset();
        SET_ERROR_AND_RETURN();
      }

      DVLOG(4) << "New NALU: " << static_cast<int>(curr_nalu_->nal_unit_type);
    }

    if (curr_nalu_->nuh_layer_id) {
      // For accelerators that support alpha layers, the data is
      // simply passed through.
      if (aux_alpha_layer_id_ == curr_nalu_->nuh_layer_id &&
          accelerator_->IsAlphaLayerSupported()) {
        switch (curr_nalu_->nal_unit_type) {
          case H265NALU::BLA_W_LP:
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
              par_res = parser_.ParseSliceHeader(
                  *curr_nalu_, curr_slice_hdr_.get(), last_slice_hdr_.get());
              if (par_res == H265Parser::kMissingParameterSet) {
                // As with the base layer, we could be trying to start decoding
                // from a bad frame, and may be able to recover later.
                curr_slice_hdr_.reset();
                last_slice_hdr_.reset();
                break;
              }
              if (par_res != H265Parser::kOk) {
                SET_ERROR_AND_RETURN();
              }
            }
            const H265PPS* pps =
                parser_.GetPPS(curr_slice_hdr_->slice_pic_parameter_set_id);
            const H265SPS* sps = parser_.GetSPS(pps->pps_seq_parameter_set_id);
            H265Picture::Vector empty;
            CHECK_ACCELERATOR_RESULT(accelerator_->SubmitSlice(
                sps, pps, curr_slice_hdr_.get(), empty, empty, empty, empty,
                empty, curr_pic_.get(), curr_slice_hdr_->nalu_data,
                curr_slice_hdr_->nalu_size, parser_.GetCurrentSubsamples()));
            last_slice_hdr_.swap(curr_slice_hdr_);
            curr_slice_hdr_.reset();
            break;
          }
          case H265NALU::SPS_NUT: {
            int sps_id;
            par_res = parser_.ParseSPS(&sps_id);
            if (par_res != H265Parser::kOk) {
              SET_ERROR_AND_RETURN();
            }
            accelerator_->ProcessSPS(
                parser_.GetSPS(sps_id),
                base::span<const uint8_t>(
                    curr_nalu_->data.get(),
                    base::checked_cast<size_t>(curr_nalu_->size)));
            break;
          }
          case H265NALU::PPS_NUT: {
            int pps_id;
            par_res = parser_.ParsePPS(*curr_nalu_, &pps_id);
            if (par_res != H265Parser::kOk) {
              SET_ERROR_AND_RETURN();
            }
            accelerator_->ProcessPPS(
                parser_.GetPPS(pps_id),
                base::span<const uint8_t>(
                    curr_nalu_->data.get(),
                    base::checked_cast<size_t>(curr_nalu_->size)));
            break;
          }
          default:
            break;
        }
      } else {
        // 8.1.2 Otherwise only handle nuh_layer_id of zero.
        DVLOG(4) << "Skipping NALU with nuh_layer_id="
                 << curr_nalu_->nuh_layer_id;
      }
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
      case H265NALU::CRA_NUT:
        if (!curr_slice_hdr_) {
          curr_slice_hdr_.reset(new H265SliceHeader());
          par_res = parser_.ParseSliceHeader(*curr_nalu_, curr_slice_hdr_.get(),
                                             last_slice_hdr_.get());
          if (par_res == H265Parser::kMissingParameterSet) {
            // We may still be able to recover if we skip until we find the
            // SPS/PPS.
            curr_slice_hdr_.reset();
            last_slice_hdr_.reset();
            break;
          }
          if (par_res != H265Parser::kOk)
            SET_ERROR_AND_RETURN();
          if (!curr_slice_hdr_->irap_pic && state_ == kAfterReset) {
            // We can't resume from a non-IRAP picture.
            curr_slice_hdr_.reset();
            last_slice_hdr_.reset();
            break;
          }

          state_ = kTryPreprocessCurrentSlice;
          if (curr_slice_hdr_->irap_pic) {
            bool need_new_buffers = false;
            if (!ProcessPPS(curr_slice_hdr_->slice_pic_parameter_set_id,
                            &need_new_buffers)) {
              SET_ERROR_AND_RETURN();
            }

            if (need_new_buffers) {
              curr_pic_ = nullptr;
              return kConfigChange;
            }
          }
        }

        if (state_ == kTryPreprocessCurrentSlice) {
          CHECK_ACCELERATOR_RESULT(PreprocessCurrentSlice());
          state_ = kEnsurePicture;
        }

        if (state_ == kEnsurePicture) {
          if (curr_pic_) {
            // |curr_pic_| already exists, so skip to ProcessCurrentSlice().
            state_ = kTryCurrentSlice;
          } else {
            // New picture, try to start a new one or tell client we need more
            // surfaces.
            if (secure_handle_) {
              curr_pic_ = accelerator_->CreateH265PictureSecure(secure_handle_);
            } else {
              curr_pic_ = accelerator_->CreateH265Picture();
            }
            if (!curr_pic_)
              return kRanOutOfSurfaces;
            if (current_decrypt_config_)
              curr_pic_->set_decrypt_config(current_decrypt_config_->Clone());
            if (hdr_metadata_.has_value())
              curr_pic_->set_hdr_metadata(hdr_metadata_);

            curr_pic_->first_picture_ = first_picture_;
            first_picture_ = false;
            state_ = kTryNewFrame;
          }
        }

        if (state_ == kTryNewFrame) {
          CHECK_ACCELERATOR_RESULT(StartNewFrame(curr_slice_hdr_.get()));
          state_ = kTryCurrentSlice;
        }

        DCHECK_EQ(state_, kTryCurrentSlice);
        CHECK_ACCELERATOR_RESULT(ProcessCurrentSlice());
        state_ = kDecoding;
        last_slice_hdr_.swap(curr_slice_hdr_);
        curr_slice_hdr_.reset();
        break;
      case H265NALU::VPS_NUT:
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        int vps_id;
        par_res = parser_.ParseVPS(&vps_id);
        if (par_res != H265Parser::kOk) {
          SET_ERROR_AND_RETURN();
        }
        // TODO(crbug.com/40937818): Technically, we should cache a map of
        // vps_id to aux_alpha_layer_id, and look up the aux_alpha_layer_id for
        // each NALU.
        aux_alpha_layer_id_ = parser_.GetVPS(vps_id)->aux_alpha_layer_id;
        accelerator_->ProcessVPS(
            parser_.GetVPS(vps_id),
            base::span<const uint8_t>(
                curr_nalu_->data.get(),
                base::checked_cast<size_t>(curr_nalu_->size)));
        break;
      case H265NALU::SPS_NUT:
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        int sps_id;
        par_res = parser_.ParseSPS(&sps_id);
        if (par_res != H265Parser::kOk)
          SET_ERROR_AND_RETURN();
        accelerator_->ProcessSPS(
            parser_.GetSPS(sps_id),
            base::span<const uint8_t>(
                curr_nalu_->data.get(),
                base::checked_cast<size_t>(curr_nalu_->size)));
        break;
      case H265NALU::PPS_NUT:
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        int pps_id;
        par_res = parser_.ParsePPS(*curr_nalu_, &pps_id);
        if (par_res != H265Parser::kOk)
          SET_ERROR_AND_RETURN();
        accelerator_->ProcessPPS(
            parser_.GetPPS(pps_id),
            base::span<const uint8_t>(
                curr_nalu_->data.get(),
                base::checked_cast<size_t>(curr_nalu_->size)));

        // For ARC CTS tests they expect us to request the buffers after only
        // processing the SPS/PPS, we can't wait until we get the first IDR. To
        // resolve the problem that was created by originally doing that, only
        // do it if we don't have an active PPS set yet so it won't disturb an
        // active stream.
        if (curr_pps_id_ == -1) {
          bool need_new_buffers = false;
          if (!ProcessPPS(pps_id, &need_new_buffers)) {
            SET_ERROR_AND_RETURN();
          }

          if (need_new_buffers) {
            curr_nalu_.reset();
            return kConfigChange;
          }
        }

        break;
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
      case H265NALU::UNSPEC55:
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        break;
      case H265NALU::PREFIX_SEI_NUT: {
        H265SEI sei;
        if (parser_.ParseSEI(&sei) != H265Parser::kOk)
          break;
        for (auto& sei_msg : sei.msgs) {
          switch (sei_msg.type) {
            case H265SEIMessage::kSEIContentLightLevelInfo:
              // HEVC HDR metadata may appears in the below places:
              // 1. Container.
              // 2. Bitstream.
              // 3. Both container and bitstream.
              // Thus we should also extract HDR metadata here in case we
              // miss the information.
              if (!hdr_metadata_.has_value()) {
                hdr_metadata_.emplace();
              }
              hdr_metadata_->cta_861_3 =
                  sei_msg.content_light_level_info.ToGfx();
              break;
            case H265SEIMessage::kSEIMasteringDisplayInfo:
              if (!hdr_metadata_.has_value()) {
                hdr_metadata_.emplace();
              }
              hdr_metadata_->smpte_st_2086 =
                  sei_msg.mastering_display_info.ToGfx();
              break;
            default:
              break;
          }
        }
        break;
      }
      default:
        DVLOG(4) << "Skipping NALU type: " << curr_nalu_->nal_unit_type;
        break;
    }

    DVLOG(4) << "NALU done";
    curr_nalu_.reset();
  }
}

gfx::Size H265Decoder::GetPicSize() const {
  return pic_size_;
}

gfx::Rect H265Decoder::GetVisibleRect() const {
  return visible_rect_;
}

VideoCodecProfile H265Decoder::GetProfile() const {
  return profile_;
}

uint8_t H265Decoder::GetBitDepth() const {
  return bit_depth_;
}

VideoChromaSampling H265Decoder::GetChromaSampling() const {
  return chroma_sampling_;
}

VideoColorSpace H265Decoder::GetVideoColorSpace() const {
  return picture_color_space_;
}
std::optional<gfx::HDRMetadata> H265Decoder::GetHDRMetadata() const {
  return hdr_metadata_;
}

size_t H265Decoder::GetRequiredNumOfPictures() const {
  constexpr size_t kPicsInPipeline = limits::kMaxVideoFrames + 1;
  return GetNumReferenceFrames() + kPicsInPipeline;
}

size_t H265Decoder::GetNumReferenceFrames() const {
  // Use the maximum number of pictures in the Decoded Picture Buffer.
  return dpb_.max_num_pics();
}

bool H265Decoder::ProcessPPS(int pps_id, bool* need_new_buffers) {
  DVLOG(4) << "Processing PPS id:" << pps_id;

  const H265PPS* pps = parser_.GetPPS(pps_id);
  // Slice header parsing already verified this should exist.
  DCHECK(pps);

  const H265SPS* sps = parser_.GetSPS(pps->pps_seq_parameter_set_id);
  // PPS parsing already verified this should exist.
  DCHECK(sps);

  if (need_new_buffers)
    *need_new_buffers = false;

  gfx::Size new_pic_size = sps->GetCodedSize();
  gfx::Rect new_visible_rect = sps->GetVisibleRect();
  if (visible_rect_ != new_visible_rect) {
    DVLOG(2) << "New visible rect: " << new_visible_rect.ToString();
    visible_rect_ = new_visible_rect;
  }

  VideoChromaSampling new_chroma_sampling = sps->GetChromaSampling();

  if (!accelerator_->IsChromaSamplingSupported(new_chroma_sampling)) {
    DVLOG(1) << "Only YUV 4:2:0 is supported";
    return false;
  }

  // Equation 7-8
  max_pic_order_cnt_lsb_ =
      std::pow(2, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);

  VideoCodecProfile new_profile = H265Parser::ProfileIDCToVideoCodecProfile(
      sps->profile_tier_level.general_profile_idc);
  if (new_profile == VIDEO_CODEC_PROFILE_UNKNOWN) {
    return false;
  }
  uint8_t new_bit_depth = 0;
  if (!ParseBitDepth(*sps, new_bit_depth)) {
    return false;
  }
  if (!IsValidBitDepth(new_bit_depth, new_profile)) {
    DVLOG(1) << "Invalid bit depth=" << base::strict_cast<int>(new_bit_depth)
             << ", profile=" << GetProfileName(new_profile);
    return false;
  }

  VideoColorSpace new_color_space;
  // For H265, prefer the frame color space over the config.
  if (sps->GetColorSpace().IsSpecified()) {
    new_color_space = sps->GetColorSpace();
  } else if (container_color_space_.IsSpecified()) {
    new_color_space = container_color_space_;
  }

  if (new_color_space.matrix == VideoColorSpace::MatrixID::RGB &&
      new_chroma_sampling != VideoChromaSampling::k444) {
    // Some H.265 videos contain a VUI that specifies a color matrix of GBR,
    // when they are actually ordinary YUV. Default to BT.709 if the format is
    // not 4:4:4 as GBR is reasonable for 4:4:4 content. See
    // crbug.com/342003180, and crbug.com/343014700.
    new_color_space = VideoColorSpace::REC709();
  }

  bool is_color_space_change = false;
  if (base::FeatureList::IsEnabled(kAVDColorSpaceChanges)) {
    is_color_space_change = new_color_space.IsSpecified() &&
                            new_color_space != picture_color_space_;
  }

  if (pic_size_ != new_pic_size || dpb_.max_num_pics() != sps->max_dpb_size ||
      profile_ != new_profile || bit_depth_ != new_bit_depth ||
      chroma_sampling_ != new_chroma_sampling || is_color_space_change) {
    if (!Flush())
      return false;
    DVLOG(1) << "Codec profile: " << GetProfileName(new_profile)
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
    picture_color_space_ = new_color_space;
    dpb_.set_max_num_pics(sps->max_dpb_size);
    if (need_new_buffers)
      *need_new_buffers = true;
  }

  return true;
}

H265Decoder::H265Accelerator::Status H265Decoder::PreprocessCurrentSlice() {
  const H265SliceHeader* slice_hdr = curr_slice_hdr_.get();
  DCHECK(slice_hdr);

  if (slice_hdr->first_slice_segment_in_pic_flag) {
    // New picture, so first finish the previous one before processing it.
    H265Accelerator::Status result = FinishPrevFrameIfPresent();
    if (result != H265Accelerator::Status::kOk)
      return result;

    DCHECK(!curr_pic_);
  }

  return H265Accelerator::Status::kOk;
}

H265Decoder::H265Accelerator::Status H265Decoder::ProcessCurrentSlice() {
  DCHECK(curr_pic_);

  const H265SliceHeader* slice_hdr = curr_slice_hdr_.get();
  DCHECK(slice_hdr);

  const H265SPS* sps = parser_.GetSPS(curr_sps_id_);
  DCHECK(sps);

  const H265PPS* pps = parser_.GetPPS(curr_pps_id_);
  DCHECK(pps);

  return accelerator_->SubmitSlice(
      sps, pps, slice_hdr, ref_pic_list0_, ref_pic_list1_, ref_pic_set_lt_curr_,
      ref_pic_set_st_curr_after_, ref_pic_set_st_curr_before_, curr_pic_.get(),
      slice_hdr->nalu_data, slice_hdr->nalu_size,
      parser_.GetCurrentSubsamples());
}

void H265Decoder::CalcPicOutputFlags(const H265SliceHeader* slice_hdr) {
  if (slice_hdr->irap_pic) {
    // 8.1.3
    curr_pic_->no_rasl_output_flag_ =
        (curr_nalu_->nal_unit_type >= H265NALU::BLA_W_LP &&
         curr_nalu_->nal_unit_type <= H265NALU::IDR_N_LP) ||
        curr_pic_->first_picture_;
    no_rasl_output_flag_ = curr_pic_->no_rasl_output_flag_;
  } else {
    curr_pic_->no_rasl_output_flag_ = no_rasl_output_flag_;
  }

  // C.5.2.2
  if (slice_hdr->irap_pic && curr_pic_->no_rasl_output_flag_ &&
      !curr_pic_->first_picture_) {
    curr_pic_->no_output_of_prior_pics_flag_ =
        (slice_hdr->nal_unit_type == H265NALU::CRA_NUT) ||
        slice_hdr->no_output_of_prior_pics_flag;
  } else {
    curr_pic_->no_output_of_prior_pics_flag_ = false;
  }

  if ((slice_hdr->nal_unit_type == H265NALU::RASL_N ||
       slice_hdr->nal_unit_type == H265NALU::RASL_R) &&
      curr_pic_->no_rasl_output_flag_) {
    curr_pic_->pic_output_flag_ = false;
  } else {
    curr_pic_->pic_output_flag_ = slice_hdr->pic_output_flag;
  }
}

void H265Decoder::CalcPictureOrderCount(const H265PPS* pps,
                                        const H265SliceHeader* slice_hdr) {
  // 8.3.1 Decoding process for picture order count.
  curr_pic_->valid_for_prev_tid0_pic_ =
      !slice_hdr->temporal_id &&
      (slice_hdr->nal_unit_type < H265NALU::RADL_N ||
       slice_hdr->nal_unit_type > H265NALU::RSV_VCL_N14);
  curr_pic_->slice_pic_order_cnt_lsb_ = slice_hdr->slice_pic_order_cnt_lsb;

  // Calculate POC for current picture.
  if ((!slice_hdr->irap_pic || !curr_pic_->no_rasl_output_flag_) &&
      prev_tid0_pic_) {
    const int prev_pic_order_cnt_lsb = prev_tid0_pic_->slice_pic_order_cnt_lsb_;
    const int prev_pic_order_cnt_msb = prev_tid0_pic_->pic_order_cnt_msb_;
    if ((slice_hdr->slice_pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
        ((prev_pic_order_cnt_lsb - slice_hdr->slice_pic_order_cnt_lsb) >=
         (max_pic_order_cnt_lsb_ / 2))) {
      curr_pic_->pic_order_cnt_msb_ =
          prev_pic_order_cnt_msb + max_pic_order_cnt_lsb_;
    } else if ((slice_hdr->slice_pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
               ((slice_hdr->slice_pic_order_cnt_lsb - prev_pic_order_cnt_lsb) >
                (max_pic_order_cnt_lsb_ / 2))) {
      curr_pic_->pic_order_cnt_msb_ =
          prev_pic_order_cnt_msb - max_pic_order_cnt_lsb_;
    } else {
      curr_pic_->pic_order_cnt_msb_ = prev_pic_order_cnt_msb;
    }
  } else {
    curr_pic_->pic_order_cnt_msb_ = 0;
  }
  curr_pic_->pic_order_cnt_val_ =
      curr_pic_->pic_order_cnt_msb_ + slice_hdr->slice_pic_order_cnt_lsb;
}

bool H265Decoder::CalcRefPicPocs(const H265SPS* sps,
                                 const H265PPS* pps,
                                 const H265SliceHeader* slice_hdr) {
  if (slice_hdr->nal_unit_type == H265NALU::IDR_W_RADL ||
      slice_hdr->nal_unit_type == H265NALU::IDR_N_LP) {
    num_poc_st_curr_before_ = num_poc_st_curr_after_ = num_poc_st_foll_ =
        num_poc_lt_curr_ = num_poc_lt_foll_ = 0;
    return true;
  }

  // 8.3.2 - NOTE 2
  const H265StRefPicSet& curr_st_ref_pic_set = slice_hdr->GetStRefPicSet(sps);

  // Equation 8-5.
  int i, j, k;
  for (i = 0, j = 0, k = 0; i < curr_st_ref_pic_set.num_negative_pics; ++i) {
    base::CheckedNumeric<int> poc = curr_pic_->pic_order_cnt_val_;
    poc += curr_st_ref_pic_set.delta_poc_s0[i];
    if (!poc.IsValid()) {
      DVLOG(1) << "Invalid POC";
      return false;
    }
    if (curr_st_ref_pic_set.used_by_curr_pic_s0[i])
      poc_st_curr_before_[j++] = poc.ValueOrDefault(0);
    else
      poc_st_foll_[k++] = poc.ValueOrDefault(0);
  }
  num_poc_st_curr_before_ = j;
  for (i = 0, j = 0; i < curr_st_ref_pic_set.num_positive_pics; ++i) {
    base::CheckedNumeric<int> poc = curr_pic_->pic_order_cnt_val_;
    poc += curr_st_ref_pic_set.delta_poc_s1[i];
    if (!poc.IsValid()) {
      DVLOG(1) << "Invalid POC";
      return false;
    }
    if (curr_st_ref_pic_set.used_by_curr_pic_s1[i])
      poc_st_curr_after_[j++] = poc.ValueOrDefault(0);
    else
      poc_st_foll_[k++] = poc.ValueOrDefault(0);
  }
  num_poc_st_curr_after_ = j;
  num_poc_st_foll_ = k;
  for (i = 0, j = 0, k = 0;
       i < slice_hdr->num_long_term_sps + slice_hdr->num_long_term_pics; ++i) {
    base::CheckedNumeric<int> poc_lt = slice_hdr->poc_lsb_lt[i];
    if (slice_hdr->delta_poc_msb_present_flag[i]) {
      poc_lt += curr_pic_->pic_order_cnt_val_;
      base::CheckedNumeric<int> poc_delta =
          slice_hdr->delta_poc_msb_cycle_lt[i];
      poc_delta *= max_pic_order_cnt_lsb_;
      if (!poc_delta.IsValid()) {
        DVLOG(1) << "Invalid POC";
        return false;
      }
      poc_lt -= poc_delta.ValueOrDefault(0);
      poc_lt -= curr_pic_->pic_order_cnt_val_ & (max_pic_order_cnt_lsb_ - 1);
    }
    if (!poc_lt.IsValid()) {
      DVLOG(1) << "Invalid POC";
      return false;
    }
    if (slice_hdr->used_by_curr_pic_lt[i]) {
      poc_lt_curr_[j] = poc_lt.ValueOrDefault(0);
      curr_delta_poc_msb_present_flag_[j++] =
          slice_hdr->delta_poc_msb_present_flag[i];
    } else {
      poc_lt_foll_[k] = poc_lt.ValueOrDefault(0);
      foll_delta_poc_msb_present_flag_[k++] =
          slice_hdr->delta_poc_msb_present_flag[i];
    }
  }
  num_poc_lt_curr_ = j;
  num_poc_lt_foll_ = k;

  // Check conformance for |num_pic_total_curr|.
  if (slice_hdr->nal_unit_type == H265NALU::CRA_NUT ||
      (slice_hdr->nal_unit_type >= H265NALU::BLA_W_LP &&
       slice_hdr->nal_unit_type <= H265NALU::BLA_N_LP)) {
    if (slice_hdr->num_pic_total_curr) {
      DVLOG(1) << "Invalid value for num_pic_total_curr";
      return false;
    }
  } else if ((slice_hdr->IsBSlice() || slice_hdr->IsPSlice()) &&
             !slice_hdr->num_pic_total_curr) {
    DVLOG(1) << "Invalid value for num_pic_total_curr";
    return false;
  }

  return true;
}

bool H265Decoder::BuildRefPicLists(const H265SPS* sps,
                                   const H265PPS* pps,
                                   const H265SliceHeader* slice_hdr) {
  ref_pic_set_lt_curr_.clear();
  ref_pic_set_lt_curr_.resize(kMaxDpbSize);
  ref_pic_set_st_curr_after_.clear();
  ref_pic_set_st_curr_after_.resize(kMaxDpbSize);
  ref_pic_set_st_curr_before_.clear();
  ref_pic_set_st_curr_before_.resize(kMaxDpbSize);
  scoped_refptr<H265Picture> ref_pic_set_lt_foll[kMaxDpbSize];
  scoped_refptr<H265Picture> ref_pic_set_st_foll[kMaxDpbSize];

  // Mark everything in the DPB as unused for reference now. When we determine
  // the pics in the ref list, then we will mark them appropriately.
  dpb_.MarkAllUnusedForReference();

  // Equation 8-6.
  // We may be missing reference pictures, if so then we just don't specify
  // them and let the accelerator deal with the missing reference pictures
  // which is covered in the spec.
  int total_ref_pics = 0;
  for (int i = 0; i < num_poc_lt_curr_; ++i) {
    if (!curr_delta_poc_msb_present_flag_[i])
      ref_pic_set_lt_curr_[i] = dpb_.GetPicByPocMaskedAndMark(
          poc_lt_curr_[i], sps->max_pic_order_cnt_lsb - 1,
          H265Picture::kLongTermCurr);
    else
      ref_pic_set_lt_curr_[i] =
          dpb_.GetPicByPocAndMark(poc_lt_curr_[i], H265Picture::kLongTermCurr);

    if (ref_pic_set_lt_curr_[i])
      total_ref_pics++;
  }
  for (int i = 0; i < num_poc_lt_foll_; ++i) {
    if (!foll_delta_poc_msb_present_flag_[i])
      ref_pic_set_lt_foll[i] = dpb_.GetPicByPocMaskedAndMark(
          poc_lt_foll_[i], sps->max_pic_order_cnt_lsb - 1,
          H265Picture::kLongTermFoll);
    else
      ref_pic_set_lt_foll[i] =
          dpb_.GetPicByPocAndMark(poc_lt_foll_[i], H265Picture::kLongTermFoll);

    if (ref_pic_set_lt_foll[i])
      total_ref_pics++;
  }

  // Equation 8-7.
  for (int i = 0; i < num_poc_st_curr_before_; ++i) {
    ref_pic_set_st_curr_before_[i] = dpb_.GetPicByPocAndMark(
        poc_st_curr_before_[i], H265Picture::kShortTermCurrBefore);

    if (ref_pic_set_st_curr_before_[i])
      total_ref_pics++;
  }
  for (int i = 0; i < num_poc_st_curr_after_; ++i) {
    ref_pic_set_st_curr_after_[i] = dpb_.GetPicByPocAndMark(
        poc_st_curr_after_[i], H265Picture::kShortTermCurrAfter);
    if (ref_pic_set_st_curr_after_[i])
      total_ref_pics++;
  }
  for (int i = 0; i < num_poc_st_foll_; ++i) {
    ref_pic_set_st_foll[i] =
        dpb_.GetPicByPocAndMark(poc_st_foll_[i], H265Picture::kShortTermFoll);
    if (ref_pic_set_st_foll[i])
      total_ref_pics++;
  }

  // Verify that the total number of reference pictures in the DPB matches the
  // total count of reference pics. This ensures that a picture is not in more
  // than one list, per the spec.
  if (dpb_.GetReferencePicCount() != total_ref_pics) {
    DVLOG(1) << "Conformance problem, reference pic is in more than one list";
    return false;
  }

  ref_pic_list_.clear();
  dpb_.AppendReferencePics(&ref_pic_list_);
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();

  // 8.3.3 Generation of unavailable reference pictures is something we do not
  // need to handle here. It's handled by the accelerator itself when we do not
  // specify a reference picture that it needs.

  if (slice_hdr->IsPSlice() || slice_hdr->IsBSlice()) {
    // 8.3.4 Decoding process for reference picture lists construction
    int num_rps_curr_temp_list0 =
        std::max(slice_hdr->num_ref_idx_l0_active_minus1 + 1,
                 slice_hdr->num_pic_total_curr);
    scoped_refptr<H265Picture> ref_pic_list_temp0[kMaxDpbSize];

    // Equation 8-8.
    int r_idx = 0;
    while (r_idx < num_rps_curr_temp_list0) {
      for (int i = 0;
           i < num_poc_st_curr_before_ && r_idx < num_rps_curr_temp_list0;
           ++i, ++r_idx) {
        ref_pic_list_temp0[r_idx] = ref_pic_set_st_curr_before_[i];
      }
      for (int i = 0;
           i < num_poc_st_curr_after_ && r_idx < num_rps_curr_temp_list0;
           ++i, ++r_idx) {
        ref_pic_list_temp0[r_idx] = ref_pic_set_st_curr_after_[i];
      }
      for (int i = 0; i < num_poc_lt_curr_ && r_idx < num_rps_curr_temp_list0;
           ++i, ++r_idx) {
        ref_pic_list_temp0[r_idx] = ref_pic_set_lt_curr_[i];
      }
    }

    // Equation 8-9.
    for (r_idx = 0; r_idx <= slice_hdr->num_ref_idx_l0_active_minus1; ++r_idx) {
      ref_pic_list0_.push_back(
          slice_hdr->ref_pic_lists_modification
                  .ref_pic_list_modification_flag_l0
              ? ref_pic_list_temp0[slice_hdr->ref_pic_lists_modification
                                       .list_entry_l0[r_idx]]
              : ref_pic_list_temp0[r_idx]);
    }

    if (slice_hdr->IsBSlice()) {
      int num_rps_curr_temp_list1 =
          std::max(slice_hdr->num_ref_idx_l1_active_minus1 + 1,
                   slice_hdr->num_pic_total_curr);
      scoped_refptr<H265Picture> ref_pic_list_temp1[kMaxDpbSize];

      // Equation 8-10.
      r_idx = 0;
      while (r_idx < num_rps_curr_temp_list1) {
        for (int i = 0;
             i < num_poc_st_curr_after_ && r_idx < num_rps_curr_temp_list1;
             ++i, r_idx++) {
          ref_pic_list_temp1[r_idx] = ref_pic_set_st_curr_after_[i];
        }
        for (int i = 0;
             i < num_poc_st_curr_before_ && r_idx < num_rps_curr_temp_list1;
             ++i, r_idx++) {
          ref_pic_list_temp1[r_idx] = ref_pic_set_st_curr_before_[i];
        }
        for (int i = 0; i < num_poc_lt_curr_ && r_idx < num_rps_curr_temp_list1;
             ++i, r_idx++) {
          ref_pic_list_temp1[r_idx] = ref_pic_set_lt_curr_[i];
        }
      }

      // Equation 8-11.
      for (r_idx = 0; r_idx <= slice_hdr->num_ref_idx_l1_active_minus1;
           ++r_idx) {
        ref_pic_list1_.push_back(
            slice_hdr->ref_pic_lists_modification
                    .ref_pic_list_modification_flag_l1
                ? ref_pic_list_temp1[slice_hdr->ref_pic_lists_modification
                                         .list_entry_l1[r_idx]]
                : ref_pic_list_temp1[r_idx]);
      }
    }
  }

  return true;
}

H265Decoder::H265Accelerator::Status H265Decoder::StartNewFrame(
    const H265SliceHeader* slice_hdr) {
  CHECK(curr_pic_.get());
  DCHECK(slice_hdr);

  curr_pps_id_ = slice_hdr->slice_pic_parameter_set_id;
  const H265PPS* pps = parser_.GetPPS(curr_pps_id_);
  // Slice header parsing already verified this should exist.
  DCHECK(pps);

  curr_sps_id_ = pps->pps_seq_parameter_set_id;
  const H265SPS* sps = parser_.GetSPS(curr_sps_id_);
  // Slice header parsing already verified this should exist.
  DCHECK(sps);

  // If this is from a retry on SubmitFrameMetadata, we should not redo all of
  // these calculations.
  if (!curr_pic_->processed_) {
    // Copy slice/pps variables we need to the picture.
    curr_pic_->nal_unit_type_ = curr_nalu_->nal_unit_type;
    curr_pic_->irap_pic_ = slice_hdr->irap_pic;

    curr_pic_->set_visible_rect(visible_rect_);
    curr_pic_->set_bitstream_id(stream_id_);

    // Set the color space for the picture.
    curr_pic_->set_colorspace(picture_color_space_);

    CalcPicOutputFlags(slice_hdr);
    CalcPictureOrderCount(pps, slice_hdr);

    if (!CalcRefPicPocs(sps, pps, slice_hdr)) {
      return H265Accelerator::Status::kFail;
    }

    if (!BuildRefPicLists(sps, pps, slice_hdr)) {
      return H265Accelerator::Status::kFail;
    }

    if (!PerformDpbOperations(sps)) {
      return H265Accelerator::Status::kFail;
    }

    curr_pic_->processed_ = true;
  }

  return accelerator_->SubmitFrameMetadata(
      sps, pps, slice_hdr, ref_pic_list_, ref_pic_set_lt_curr_,
      ref_pic_set_st_curr_after_, ref_pic_set_st_curr_before_, curr_pic_);
}

H265Decoder::H265Accelerator::Status H265Decoder::FinishPrevFrameIfPresent() {
  // If we don't already have a frame waiting to be decoded, do nothing.
  if (!curr_pic_) {
    return H265Accelerator::Status::kOk;
  }

  // If there is no slice header (eg. because the last one was dropped due to
  // a missing PPS), this picture can't be decoded.
  if (!last_slice_hdr_) {
    return H265Accelerator::Status::kFail;
  }

  H265Accelerator::Status result = DecodePicture();
  if (result != H265Accelerator::Status::kOk) {
    return result;
  }

  if (!FinishPicture(std::move(curr_pic_), std::move(last_slice_hdr_))) {
    return H265Accelerator::Status::kFail;
  }

  return H265Accelerator::Status::kOk;
}

bool H265Decoder::PerformDpbOperations(const H265SPS* sps) {
  // C.5.2.2 - Output and removal of pictures from the DPB
  if (curr_pic_->irap_pic_ && curr_pic_->no_rasl_output_flag_ &&
      !curr_pic_->first_picture_) {
    if (!curr_pic_->no_output_of_prior_pics_flag_) {
      OutputAllRemainingPics();
    }
    dpb_.Clear();
  } else {
    int num_to_output;
    do {
      dpb_.DeleteUnused();
      // Get all pictures that haven't been outputted yet.
      H265Picture::Vector not_outputted;
      dpb_.AppendPendingOutputPics(&not_outputted);
      // Sort in output order.
      std::sort(not_outputted.begin(), not_outputted.end(), POCAscCompare());

      // Calculate how many pictures we need to output.
      num_to_output = 0;
      int highest_tid = sps->sps_max_sub_layers_minus1;

      // C.5.2.2 - "The number of pictures in the DPB that are marked as "needed
      // for output" is greater than sps_max_num_reorder_pics[ HighestTid ]."
      num_to_output = std::max(num_to_output,
                               static_cast<int>(not_outputted.size()) -
                                   sps->sps_max_num_reorder_pics[highest_tid]);

      // C.5.2.2 - "The number of pictures in the DPB is greater than or equal
      // to sps_max_dec_pic_buffering_minus1[ HighestTid ] + 1 −
      // TwoVersionsOfCurrDecPicFlag."
      num_to_output =
          std::max(num_to_output,
                   static_cast<int>(dpb_.size()) -
                       sps->sps_max_dec_pic_buffering_minus1[highest_tid]);

      // C.5.2.2 - "sps_max_latency_increase_plus1[ HighestTid ] is not equal to
      // 0 and there is at least one picture in the DPB that is marked as
      // "needed for output" for which the associated variable PicLatencyCount
      // is greater than or equal to SpsMaxLatencyPictures[ HighestTid ]."
      int pic_latency_output_count = 0;
      if (sps->sps_max_latency_increase_plus1[highest_tid] != 0) {
        for (const auto& pic : not_outputted) {
          if (pic->pic_latency_count_ >=
              sps->sps_max_latency_pictures[highest_tid]) {
            ++pic_latency_output_count;
          }
        }
      }
      num_to_output = std::max(num_to_output, pic_latency_output_count);

      num_to_output =
          std::min(num_to_output, static_cast<int>(not_outputted.size()));

      if (!num_to_output && dpb_.IsFull()) {
        // This is wrong, we should try to output pictures until we can clear
        // one from the DPB. This is better than failing, but we then may end up
        // with something out of order.
        DVLOG(1) << "Forcibly outputting pictures to make room in DPB.";
        for (const auto& pic : not_outputted) {
          num_to_output++;
          if (pic->ref_ == H265Picture::kUnused)
            break;
        }
      }

      not_outputted.resize(num_to_output);
      for (auto& pic : not_outputted) {
        if (!OutputPic(pic))
          return false;
      }

      dpb_.DeleteUnused();
    } while (dpb_.IsFull() && num_to_output);
  }

  if (dpb_.IsFull()) {
    DVLOG(1) << "Could not free up space in DPB for current picture";
    return false;
  }

  // Put the current pic in the DPB.
  dpb_.StorePicture(curr_pic_, H265Picture::kShortTermFoll);
  return true;
}

bool H265Decoder::FinishPicture(scoped_refptr<H265Picture> pic,
                                std::unique_ptr<H265SliceHeader> slice_hdr) {
  // 8.3.1
  if (pic->valid_for_prev_tid0_pic_)
    prev_tid0_pic_ = pic;

  int pps_id = slice_hdr->slice_pic_parameter_set_id;
  const H265PPS* pps = parser_.GetPPS(pps_id);
  // Slice header parsing already verified this should exist.
  DCHECK(pps);

  int sps_id = pps->pps_seq_parameter_set_id;
  const H265SPS* sps = parser_.GetSPS(sps_id);
  // Slice header parsing already verified this should exist.
  DCHECK(sps);

  // C.5.2.3 - Additional bumping
  if (pic->pic_output_flag_) {
    // C.5.2.3 - "When the current picture has PicOutputFlag equal to 1, for
    // each picture in the DPB that is marked as "needed for output" and follows
    // the current picture in output order, the associated variable
    // PicLatencyCount is set equal to PicLatencyCount + 1."
    H265Picture::Vector to_output;
    dpb_.AppendPendingOutputPics(&to_output);
    for (const auto& pending_output_pic : to_output) {
      if (pic->pic_order_cnt_val_ < pending_output_pic->pic_order_cnt_val_) {
        ++pending_output_pic->pic_latency_count_;
      }
    }

    // C.5.2.3 - "If the current decoded picture has PicOutputFlag equal to 1,
    // it is marked as "needed for output" and its associated variable
    // PicLatencyCount is set equal to 0."
    pic->pic_latency_count_ = 0;
  }

  // Get all pictures that haven't been outputted yet.
  H265Picture::Vector not_outputted;
  dpb_.AppendPendingOutputPics(&not_outputted);

  // Sort in output order.
  std::sort(not_outputted.begin(), not_outputted.end(), POCAscCompare());

  // C.5.2.3 - "When one or more of the following conditions are true, the
  // "bumping" process specified in clause C.5.2.4 is invoked repeatedly until
  // none of the following conditions are true:

  // C.5.2.3 - "The number of pictures in the DPB that are marked as "needed
  // for output" is greater than sps_max_num_reorder_pics[ HighestTid ]."
  int num_to_output = 0;
  int highest_tid = sps->sps_max_sub_layers_minus1;
  num_to_output =
      std::max(num_to_output, static_cast<int>(not_outputted.size()) -
                                  sps->sps_max_num_reorder_pics[highest_tid]);

  // C.5.2.3 - "sps_max_latency_increase_plus1[ HighestTid ] is not equal to 0
  // and there is at least one picture in the DPB that is marked as "needed for
  // output" for which the associated variable PicLatencyCount that is greater
  // than or equal to SpsMaxLatencyPictures[ HighestTid ]."
  int pic_latency_output_count = 0;
  if (sps->sps_max_latency_increase_plus1[highest_tid] != 0) {
    for (auto& pending_output_pic : not_outputted) {
      if (pending_output_pic->pic_latency_count_ >=
          sps->sps_max_latency_pictures[highest_tid]) {
        ++pic_latency_output_count;
      }
    }
  }
  num_to_output = std::max(num_to_output, pic_latency_output_count);

  // C.5.2.4 - "Bumping" process
  num_to_output =
      std::min(num_to_output, static_cast<int>(not_outputted.size()));
  not_outputted.resize(num_to_output);
  for (auto& pending_output_pic : not_outputted) {
    if (!OutputPic(pending_output_pic)) {
      return false;
    }
  }
  dpb_.DeleteUnused();

  ref_pic_list_.clear();
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();
  ref_pic_set_lt_curr_.clear();
  ref_pic_set_st_curr_after_.clear();
  ref_pic_set_st_curr_before_.clear();

  secure_handle_ = 0;

  return true;
}

H265Decoder::H265Accelerator::Status H265Decoder::DecodePicture() {
  DCHECK(curr_pic_.get());
  return accelerator_->SubmitDecode(curr_pic_);
}

bool H265Decoder::OutputPic(scoped_refptr<H265Picture> pic) {
  DCHECK(!pic->outputted_);
  pic->outputted_ = true;

  DVLOG(4) << "Posting output task for POC: " << pic->pic_order_cnt_val_;
  return accelerator_->OutputPicture(std::move(pic));
}

bool H265Decoder::OutputAllRemainingPics() {
  // Output all pictures that are waiting to be outputted.
  H265Picture::Vector to_output;
  dpb_.AppendPendingOutputPics(&to_output);
  // Sort them by ascending POC to output in order.
  std::sort(to_output.begin(), to_output.end(), POCAscCompare());

  for (auto& pic : to_output) {
    if (!OutputPic(std::move(pic)))
      return false;
  }
  return true;
}

bool H265Decoder::Flush() {
  DVLOG(2) << "Decoder flush";

  if (!OutputAllRemainingPics())
    return false;

  dpb_.Clear();
  prev_tid0_pic_ = nullptr;
  return true;
}

}  // namespace media
