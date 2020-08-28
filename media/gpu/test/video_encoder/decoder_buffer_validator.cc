// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/decoder_buffer_validator.h"

#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/gpu/h264_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {
namespace {
int VideoCodecProfileToH264ProfileIDC(VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return H264SPS::kProfileIDCBaseline;
    case H264PROFILE_MAIN:
      return H264SPS::kProfileIDCMain;
    case H264PROFILE_HIGH:
      return H264SPS::kProfileIDCHigh;
    default:
      LOG(ERROR) << "Unexpected video profile: " << GetProfileName(profile);
  }
  return H264SPS::kProfileIDCMain;
}

int VideoCodecProfileToVP9Profile(VideoCodecProfile profile) {
  switch (profile) {
    case VP9PROFILE_PROFILE0:
      return 0;
    default:
      LOG(ERROR) << "Unexpected video profile: " << GetProfileName(profile);
  }
  return 0;
}
}  // namespace

DecoderBufferValidator::DecoderBufferValidator(const gfx::Rect& visible_rect)
    : visible_rect_(visible_rect) {}

DecoderBufferValidator::~DecoderBufferValidator() = default;

void DecoderBufferValidator::ProcessBitstream(
    scoped_refptr<BitstreamRef> bitstream,
    size_t frame_index) {
  if (!Validate(*bitstream->buffer, bitstream->metadata))
    num_errors_++;
}

bool DecoderBufferValidator::WaitUntilDone() {
  return num_errors_ == 0;
}

TemporalLayerValidator::TemporalLayerValidator(size_t num_temporal_layers)
    : num_temporal_layers_(num_temporal_layers) {
  reference_frames_.fill(0);
}

TemporalLayerValidator::~TemporalLayerValidator() = default;

bool TemporalLayerValidator::ValidateAndUpdate(bool keyframe,
                                               uint8_t temporal_index,
                                               uint8_t reference_index,
                                               uint8_t refresh_frame_index) {
  if (temporal_index >= num_temporal_layers_) {
    LOG(ERROR) << "Temporal layer index is not less than the number of temporal"
               << " layers, temporal_index=" << temporal_index
               << ", num_temporal_layers=" << num_temporal_layers_;
    return false;
  }
  if (keyframe) {
    if (temporal_index != 0) {
      LOG(ERROR) << "Key frame exists in non base layer, temporal_index="
                 << temporal_index;
      return false;
    }
    reference_frames_.fill(temporal_index);
    return true;
  }

  const std::bitset<kReferenceFramePoolSize> reference(reference_index);
  for (size_t i = 0; i < kReferenceFramePoolSize; ++i) {
    if (!reference[i])
      continue;
    const uint8_t referenced_index = reference_frames_[i];
    if (referenced_index > temporal_index) {
      LOG(ERROR) << "Frame in upper layer referenced, temporal_index="
                 << temporal_index
                 << ", referenced temporal index=" << referenced_index;
      return false;
    }
  }
  const std::bitset<kReferenceFramePoolSize> refresh(refresh_frame_index);
  for (size_t i = 0; i < kReferenceFramePoolSize; ++i) {
    if (refresh[i])
      reference_frames_[i] = temporal_index;
  }
  return true;
}

H264Validator::H264Validator(VideoCodecProfile profile,
                             const gfx::Rect& visible_rect,
                             base::Optional<uint8_t> level)
    : DecoderBufferValidator(visible_rect),
      cur_pic_(new H264Picture),
      profile_(VideoCodecProfileToH264ProfileIDC(profile)),
      level_(level) {}

H264Validator::~H264Validator() = default;

bool H264Validator::Validate(const DecoderBuffer& decoder_buffer,
                             const BitstreamBufferMetadata& metadata) {
  parser_.SetStream(decoder_buffer.data(), decoder_buffer.data_size());

  size_t num_frames = 0;
  H264NALU nalu;
  H264Parser::Result result;
  while ((result = parser_.AdvanceToNextNALU(&nalu)) != H264Parser::kEOStream) {
    if (result != H264Parser::kOk) {
      LOG(ERROR) << "Failed parsing";
      return false;
    }

    switch (nalu.nal_unit_type) {
      case H264NALU::kIDRSlice:
        if (!seen_sps_ || !seen_pps_) {
          LOG(ERROR) << "IDR frame before SPS and PPS";
          return false;
        }
        seen_idr_ = true;
        FALLTHROUGH;
      case H264NALU::kNonIDRSlice: {
        if (!seen_idr_) {
          LOG(ERROR) << "Non IDR frame before IDR frame";
          return false;
        }

        H264SliceHeader slice_hdr;
        if (parser_.ParseSliceHeader(nalu, &slice_hdr) != H264Parser::kOk) {
          LOG(ERROR) << "Failed parsing slice";
          return false;
        }
        // TODO(hiroh): Add more checks.
        if (IsNewPicture(slice_hdr)) {
          // A new frame is found. Initialize |cur_pic|.
          num_frames++;
          if (!UpdateCurrentPicture(slice_hdr))
            return false;
        }
        break;
      }
      case H264NALU::kSPS: {
        int sps_id;
        if (parser_.ParseSPS(&sps_id) != H264Parser::kOk) {
          LOG(ERROR) << "Failed parsing SPS";
          return false;
        }

        // Check the visible rect.
        const H264SPS* sps = parser_.GetSPS(sps_id);
        const auto& visible_rect = sps->GetVisibleRect().value_or(gfx::Rect());
        if (visible_rect != visible_rect_) {
          LOG(ERROR) << "Visible rectangle mismatched. Actual visible_rect: "
                     << visible_rect.ToString()
                     << ", expected visible_rect: " << visible_rect_.ToString();
          return false;
        }
        if (profile_ != sps->profile_idc) {
          LOG(ERROR) << "Profile mismatched. Actual profile: "
                     << sps->profile_idc << ", expected profile: " << profile_;
          return false;
        }
        if (level_ && sps->GetIndicatedLevel() != *level_) {
          LOG(ERROR) << "Level mismatched. Actual profile: "
                     << static_cast<int>(sps->GetIndicatedLevel())
                     << ", expected profile: " << static_cast<int>(*level_);
          return false;
        }

        seen_sps_ = true;
        break;
      }
      case H264NALU::kPPS: {
        if (!seen_sps_) {
          LOG(ERROR) << "PPS before SPS";
          return false;
        }
        int pps_id;
        if (parser_.ParsePPS(&pps_id) != H264Parser::kOk) {
          LOG(ERROR) << "Failed parsing PPS";
          return false;
        }
        seen_pps_ = true;
        break;
      }
      default:
        break;
    }
  }

  return num_frames == 1u;
}

bool H264Validator::IsNewPicture(const H264SliceHeader& slice_hdr) {
  if (!cur_pic_)
    return true;
  return H264Decoder::IsNewPrimaryCodedPicture(
      cur_pic_.get(), cur_pps_id_, parser_.GetSPS(cur_sps_id_), slice_hdr);
}

bool H264Validator::UpdateCurrentPicture(const H264SliceHeader& slice_hdr) {
  cur_pps_id_ = slice_hdr.pic_parameter_set_id;
  const H264PPS* pps = parser_.GetPPS(cur_pps_id_);
  if (!pps) {
    LOG(ERROR) << "Cannot parse pps.";
    return false;
  }

  cur_sps_id_ = pps->seq_parameter_set_id;
  const H264SPS* sps = parser_.GetSPS(cur_sps_id_);
  if (!sps) {
    LOG(ERROR) << "Cannot parse sps.";
    return false;
  }

  if (!H264Decoder::FillH264PictureFromSliceHeader(sps, slice_hdr,
                                                   cur_pic_.get())) {
    LOG(ERROR) << "Cannot initialize current frame.";
    return false;
  }
  return true;
}

VP8Validator::VP8Validator(const gfx::Rect& visible_rect)
    : DecoderBufferValidator(visible_rect) {}

VP8Validator::~VP8Validator() = default;

bool VP8Validator::Validate(const DecoderBuffer& decoder_buffer,
                            const BitstreamBufferMetadata& metadata) {
  // TODO(hiroh): We could be getting more frames in the buffer, but there is
  // no simple way to detect this. We'd need to parse the frames and go through
  // partition numbers/sizes. For now assume one frame per buffer.
  Vp8FrameHeader header;
  if (!parser_.ParseFrame(decoder_buffer.data(), decoder_buffer.data_size(),
                          &header)) {
    LOG(ERROR) << "Failed parsing";
    return false;
  }

  if (header.IsKeyframe()) {
    seen_keyframe_ = true;
    if (gfx::Rect(header.width, header.height) != visible_rect_) {
      LOG(ERROR) << "Visible rectangle mismatched. Actual visible_rect: "
                 << gfx::Rect(header.width, header.height).ToString()
                 << ", expected visible_rect: " << visible_rect_.ToString();
      return false;
    }
  }

  return seen_keyframe_ && header.show_frame;
}

VP9Validator::VP9Validator(VideoCodecProfile profile,
                           const gfx::Rect& visible_rect,
                           size_t num_temporal_layers)
    : DecoderBufferValidator(visible_rect),
      parser_(false /* parsing_compressed_header */),
      profile_(VideoCodecProfileToVP9Profile(profile)),
      temporal_layer_validator_(
          num_temporal_layers > 1u
              ? std::make_unique<TemporalLayerValidator>(num_temporal_layers)
              : nullptr) {}

VP9Validator::~VP9Validator() = default;

bool VP9Validator::Validate(const DecoderBuffer& decoder_buffer,
                            const BitstreamBufferMetadata& metadata) {
  // TODO(hiroh): We could be getting more frames in the buffer, but there is
  // no simple way to detect this. We'd need to parse the frames and go through
  // partition numbers/sizes. For now assume one frame per buffer.
  Vp9FrameHeader header;
  gfx::Size allocate_size;
  parser_.SetStream(decoder_buffer.data(), decoder_buffer.data_size(), nullptr);
  if (parser_.ParseNextFrame(&header, &allocate_size, nullptr) ==
      Vp9Parser::kInvalidStream) {
    LOG(ERROR) << "Failed parsing";
    return false;
  }
  if (metadata.key_frame != header.IsKeyframe()) {
    LOG(ERROR) << "Keyframe info in metadata is wrong, metadata.keyframe="
               << metadata.key_frame;
    return false;
  }

  if (header.IsKeyframe()) {
    seen_keyframe_ = true;
    if (header.profile != static_cast<uint8_t>(profile_)) {
      LOG(ERROR) << "Profile mismatched. Actual profile: "
                 << static_cast<int>(header.profile)
                 << ", expected profile: " << profile_;
      return false;
    }
    if (gfx::Rect(header.render_width, header.render_height) != visible_rect_) {
      LOG(ERROR)
          << "Visible rectangle mismatched. Actual visible_rect: "
          << gfx::Rect(header.render_width, header.render_height).ToString()
          << ", expected visible_rect: " << visible_rect_.ToString();
      return false;
    }
  }

  if (!seen_keyframe_) {
    LOG(ERROR) << "First frame is not key frame";
    return false;
  }

  if (!header.show_frame) {
    LOG(ERROR) << "VideoEncodeAccelerator outputs non showable frame";
    return false;
  }

  if (!temporal_layer_validator_)
    return true;

  if (!metadata.vp9.has_value()) {
    LOG(ERROR) << "No metadata in temporal layer encoding";
    return false;
  }
  uint8_t reference_index = 0;
  for (size_t i = 0; i < kVp9NumRefsPerFrame; ++i) {
    uint8_t ref_frame_index = header.ref_frame_idx[i];
    if (ref_frame_index >= static_cast<uint8_t>(kVp9NumRefFrames)) {
      LOG(ERROR) << "Invalid reference frame index: "
                 << static_cast<int>(ref_frame_index);
      return false;
    }
    reference_index |= (1u << ref_frame_index);
  }
  return temporal_layer_validator_->ValidateAndUpdate(
      header.IsKeyframe(), metadata.vp9->temporal_idx, reference_index,
      header.refresh_frame_flags);
}
}  // namespace test
}  // namespace media
