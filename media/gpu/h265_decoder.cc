// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "media/base/limits.h"
#include "media/gpu/h265_decoder.h"

namespace media {

H265Decoder::H265Accelerator::H265Accelerator() = default;

H265Decoder::H265Accelerator::~H265Accelerator() = default;

H265Decoder::H265Accelerator::Status H265Decoder::H265Accelerator::SetStream(
    base::span<const uint8_t> stream,
    const DecryptConfig* decrypt_config) {
  return H265Decoder::H265Accelerator::Status::kNotSupported;
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
  const size_t size = decoder_buffer.data_size();
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
}

void H265Decoder::Reset() {
  curr_pic_ = nullptr;
  curr_nalu_ = nullptr;
  curr_slice_hdr_ = nullptr;
  last_slice_hdr_ = nullptr;
  curr_sps_id_ = -1;
  curr_pps_id_ = -1;

  prev_tid0_pic_ = nullptr;
  ref_pic_list_.clear();
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();

  dpb_.Clear();
  parser_.Reset();
  accelerator_->Reset();

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
        base::span<const uint8_t>(current_stream_, current_stream_size_),
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

    // 8.1.2 We only want nuh_layer_id of zero.
    if (curr_nalu_->nuh_layer_id) {
      DVLOG(4) << "Skipping NALU with nuh_layer_id="
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
      case H265NALU::CRA_NUT:
        if (!curr_slice_hdr_) {
          curr_slice_hdr_.reset(new H265SliceHeader());
          if (last_slice_hdr_) {
            // This is a multi-slice picture, so we should copy all of the prior
            // slice header data to the new slice and use those as the default
            // values that don't have syntax elements present.
            memcpy(curr_slice_hdr_.get(), last_slice_hdr_.get(),
                   sizeof(H265SliceHeader));
            last_slice_hdr_.reset();
          }
          par_res =
              parser_.ParseSliceHeader(*curr_nalu_, curr_slice_hdr_.get());
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
          if (curr_slice_hdr_->slice_pic_parameter_set_id != curr_pps_id_) {
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
            curr_pic_ = accelerator_->CreateH265Picture();
            if (!curr_pic_)
              return kRanOutOfSurfaces;
            if (current_decrypt_config_)
              curr_pic_->set_decrypt_config(current_decrypt_config_->Clone());

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
      case H265NALU::SPS_NUT:
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        int sps_id;
        par_res = parser_.ParseSPS(&sps_id);
        if (par_res != H265Parser::kOk)
          SET_ERROR_AND_RETURN();

        break;
      case H265NALU::PPS_NUT:
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        int pps_id;
        par_res = parser_.ParsePPS(*curr_nalu_, &pps_id);
        if (par_res != H265Parser::kOk)
          SET_ERROR_AND_RETURN();

        break;
      case H265NALU::EOS_NUT:
        first_picture_ = true;
        FALLTHROUGH;
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

  // Equation 7-8
  max_pic_order_cnt_lsb_ =
      std::pow(2, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);

  VideoCodecProfile new_profile = H265Parser::ProfileIDCToVideoCodecProfile(
      sps->profile_tier_level.general_profile_idc);

  if (pic_size_ != new_pic_size || dpb_.max_num_pics() != sps->max_dpb_size ||
      profile_ != new_profile) {
    if (!Flush())
      return false;
    DVLOG(1) << "Codec profile: " << GetProfileName(new_profile)
             << ", level(x30): " << sps->profile_tier_level.general_level_idc
             << ", DPB size: " << sps->max_dpb_size
             << ", Picture size: " << new_pic_size.ToString();
    profile_ = new_profile;
    pic_size_ = new_pic_size;
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
  return accelerator_->SubmitSlice(sps, pps, slice_hdr, ref_pic_list0_,
                                   ref_pic_list1_, curr_pic_.get(),
                                   slice_hdr->nalu_data, slice_hdr->nalu_size,
                                   parser_.GetCurrentSubsamples());
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

  // Copy slice/pps variables we need to the picture.
  curr_pic_->nal_unit_type_ = curr_nalu_->nal_unit_type;
  curr_pic_->irap_pic_ = slice_hdr->irap_pic;

  curr_pic_->set_visible_rect(visible_rect_);
  curr_pic_->set_bitstream_id(stream_id_);
  if (sps->GetColorSpace().IsSpecified())
    curr_pic_->set_colorspace(sps->GetColorSpace());
  else
    curr_pic_->set_colorspace(container_color_space_);

  // TODO(jkardatzke): Add calculation of picture output flags, POC,
  // ref pic POCs, building ref pic lists and dpb operations.

  return accelerator_->SubmitFrameMetadata(sps, pps, slice_hdr, ref_pic_list_,
                                           curr_pic_);
}

H265Decoder::H265Accelerator::Status H265Decoder::FinishPrevFrameIfPresent() {
  // If we already have a frame waiting to be decoded, decode it and finish.
  if (curr_pic_) {
    H265Accelerator::Status result = DecodePicture();
    if (result != H265Accelerator::Status::kOk)
      return result;

    scoped_refptr<H265Picture> pic = curr_pic_;
    curr_pic_ = nullptr;
    FinishPicture(pic);
  }

  return H265Accelerator::Status::kOk;
}

void H265Decoder::FinishPicture(scoped_refptr<H265Picture> pic) {
  // 8.3.1
  if (pic->valid_for_prev_tid0_pic_)
    prev_tid0_pic_ = pic;

  ref_pic_list_.clear();
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();

  last_slice_hdr_.reset();
}

H265Decoder::H265Accelerator::Status H265Decoder::DecodePicture() {
  DCHECK(curr_pic_.get());
  return accelerator_->SubmitDecode(curr_pic_);
}

bool H265Decoder::Flush() {
  DVLOG(2) << "Decoder flush";

  dpb_.Clear();
  prev_tid0_pic_ = nullptr;
  return true;
}

}  // namespace media
