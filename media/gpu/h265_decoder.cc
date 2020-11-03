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
  curr_nalu_ = nullptr;

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

    bool need_new_buffers;
    switch (curr_nalu_->nal_unit_type) {
      case H265NALU::SPS_NUT:
        int sps_id;
        par_res = parser_.ParseSPS(&sps_id);
        if (par_res != H265Parser::kOk)
          SET_ERROR_AND_RETURN();

        break;
      case H265NALU::PPS_NUT:
        int pps_id;
        par_res = parser_.ParsePPS(*curr_nalu_, &pps_id);
        if (par_res != H265Parser::kOk)
          SET_ERROR_AND_RETURN();

        if (!ProcessPPS(pps_id, &need_new_buffers))
          SET_ERROR_AND_RETURN();

        if (need_new_buffers)
          return kConfigChange;

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

bool H265Decoder::Flush() {
  DVLOG(2) << "Decoder flush";
  return true;
}

}  // namespace media
