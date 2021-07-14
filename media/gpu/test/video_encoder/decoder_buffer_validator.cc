// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/decoder_buffer_validator.h"

#include <set>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
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

H264Validator::H264Validator(VideoCodecProfile profile,
                             const gfx::Rect& visible_rect,
                             absl::optional<uint8_t> level)
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

        if (IsNewPicture(slice_hdr)) {
          // A new frame is found. Initialize |cur_pic|.
          num_frames++;
          if (!UpdateCurrentPicture(slice_hdr))
            return false;
        }

        if (slice_hdr.disable_deblocking_filter_idc != 0) {
          LOG(ERROR) << "Deblocking filter is not enabled";
          return false;
        }

        if (slice_hdr.slice_type == H264SliceHeader::Type::kBSlice) {
          LOG(ERROR) << "Found B slice";
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

        const H264PPS* pps = parser_.GetPPS(pps_id);
        if ((profile_ == H264SPS::kProfileIDCMain ||
             profile_ == H264SPS::kProfileIDCHigh) &&
            !pps->entropy_coding_mode_flag) {
          // CABAC must be selected if a profile is Main and High.
          LOG(ERROR) << "The entropy coding is not CABAC";
          return false;
        }

        // 8x8 transform should be enabled if a profile is High. However, we
        // don't check it because it is not enabled due to a hardware limitation
        // on AMD stoneyridge and picasso.

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
                           size_t num_spatial_layers,
                           size_t num_temporal_layers)
    : DecoderBufferValidator(visible_rect),
      parser_(/*parsing_compressed_header=*/false),
      profile_(VideoCodecProfileToVP9Profile(profile)),
      num_spatial_layers_(num_spatial_layers),
      num_temporal_layers_(num_temporal_layers),
      next_picture_id_(0) {}

VP9Validator::~VP9Validator() = default;

bool VP9Validator::Validate(const DecoderBuffer& decoder_buffer,
                            const BitstreamBufferMetadata& metadata) {
  constexpr uint8_t kSuperFrameMarker = 0b11000110;
  if ((decoder_buffer.data()[decoder_buffer.data_size() - 1] &
       kSuperFrameMarker) == kSuperFrameMarker) {
    LOG(ERROR) << "Support for super-frames not yet implemented.";
    return false;
  }

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

  if (next_picture_id_ == 0 &&
      (num_spatial_layers_ == 1 ||
       (num_spatial_layers_ > 1 && metadata.vp9->spatial_idx == 0)) &&
      !header.IsKeyframe()) {
    LOG(ERROR) << "First frame must be a key-frame.";
    return false;
  }

  BufferState new_buffer_state{};
  if (num_spatial_layers_ > 1 || num_temporal_layers_ > 1) {
    if (!metadata.vp9) {
      LOG(ERROR) << "Metadata must be populated if spatial/temporal "
                    "scalability is used.";
      return false;
    }
    if (!header.error_resilient_mode) {
      LOG(ERROR) << "Error resilient mode must be used if spatial or temporal "
                    "scaliblity is enabled.";
      return false;
    }
    new_buffer_state.spatial_id = metadata.vp9->spatial_idx;
    new_buffer_state.temporal_id = metadata.vp9->temporal_idx;
    new_buffer_state.picture_id = next_picture_id_;
    if (metadata.vp9->spatial_idx == num_spatial_layers_ - 1)
      next_picture_id_++;
  } else {
    new_buffer_state.picture_id = next_picture_id_++;
  }

  if (metadata.vp9 &&
      metadata.vp9->has_reference != !metadata.vp9->p_diffs.empty()) {
    LOG(ERROR)
        << "Inconsistent metadata, has_reference implies p_diffs is non-empty.";
    return false;
  }

  if (header.IsKeyframe()) {
    if (header.profile != static_cast<uint8_t>(profile_)) {
      LOG(ERROR) << "Profile mismatched. Actual profile: "
                 << static_cast<int>(header.profile)
                 << ", expected profile: " << profile_;
      return false;
    }
    if ((!metadata.vp9.has_value() ||
         metadata.vp9->spatial_layer_resolutions.size() == 1u) &&
        gfx::Rect(header.render_width, header.render_height) != visible_rect_) {
      LOG(ERROR)
          << "Visible rectangle mismatched. Actual visible_rect: "
          << gfx::Rect(header.render_width, header.render_height).ToString()
          << ", expected visible_rect: " << visible_rect_.ToString();
      return false;
    }

    if (new_buffer_state.spatial_id != 0 || new_buffer_state.temporal_id != 0) {
      LOG(ERROR) << "Spatial and temporal id must be 0 for key-frames.";
      return false;
    }

    new_buffer_state.picture_id = 0;
    if (!metadata.vp9 || metadata.vp9->spatial_idx == num_spatial_layers_ - 1)
      next_picture_id_ = 1;
  } else if (header.show_existing_frame) {
    if (!reference_buffers_[header.frame_to_show_map_idx]) {
      LOG(ERROR) << "Attempting to show an existing frame, but the selected "
                    "reference buffer is invalid.";
      return false;
    }
    // No decoder state is updated if showing existing frame, but the picture id
    // is still incremented.
    if (metadata.vp9) {
      int expected_diff =
          new_buffer_state.picture_id -
          reference_buffers_[header.frame_to_show_map_idx]->picture_id;
      if (metadata.vp9->p_diffs.size() != 1 ||
          metadata.vp9->p_diffs[0] != expected_diff) {
        LOG(ERROR)
            << "Inconsistency between p_diff and existing frame to show.";
        return false;
      }
    }

    return true;
  }

  // Check that referenced frames are OK.
  if (header.IsIntra()) {
    if (metadata.vp9 && !metadata.vp9->p_diffs.empty()) {
      // TODO(crbug.com/1186051): Consider if this is truly an error-state.
      LOG(ERROR) << "|p_diffs| should be empty in intra-frames.";
      return false;
    }
  } else {
    std::vector<int> expected_pdiffs;
    std::set<uint8_t> used_indices;
    for (uint8_t ref_frame_index : header.ref_frame_idx) {
      if (ref_frame_index >= static_cast<uint8_t>(kVp9NumRefFrames)) {
        LOG(ERROR) << "Invalid reference frame index: "
                   << static_cast<int>(ref_frame_index);
        return false;
      }

      if (base::Contains(used_indices, ref_frame_index)) {
        // |header.ref_frame_index| might have the same indices because an
        // encoder fills the same index if the actually used ref frames is less
        // than |kVp9NumRefsPerFrame|.
        continue;
      }

      used_indices.insert(ref_frame_index);

      if (!reference_buffers_[ref_frame_index]) {
        LOG(ERROR) << "Frame is trying to reference buffer with invalid state.";
        return false;
      }
      const BufferState& ref = *reference_buffers_[ref_frame_index];
      if (ref.spatial_id > new_buffer_state.spatial_id) {
        LOG(ERROR)
            << "Frame is trying to reference buffer from higher spatial layer.";
        return false;
      }
      if (ref.temporal_id > new_buffer_state.temporal_id) {
        LOG(ERROR) << "Frame is trying to reference buffer from higher "
                      "temporal layer.";
        return false;
      }

      // For key picture (|new_buffer_state.picture_id| == 0), we don't fill
      // |p_diffs| even though it reference lower spatial layer frame. Skip
      // inserting |expected_pdiffs|.
      if (new_buffer_state.picture_id == 0)
        continue;

      expected_pdiffs.push_back(new_buffer_state.picture_id - ref.picture_id);
    }
    if (metadata.vp9) {
      for (uint8_t p_diff : metadata.vp9->p_diffs) {
        if (!base::Erase(expected_pdiffs, p_diff)) {
          LOG(ERROR)
              << "Frame is referencing buffer not contained in the p_diff.";
          return false;
        }
      }
      if (!expected_pdiffs.empty()) {
        // TODO(crbug.com/1186051): Consider if this is truly an error-state.
        LOG(ERROR)
            << "|p_diff| contains frame that is not actually referenced.";
        return false;
      }
    }
  }

  if (metadata.vp9 && metadata.vp9->temporal_up_switch) {
    // Temporal up-switch, invalidate any buffers containing frames with higher
    // temporal id.
    for (auto& buffer : reference_buffers_) {
      if (buffer && buffer->temporal_id > new_buffer_state.temporal_id) {
        buffer.reset();
      }
    }
  }

  // Update current state with the new buffer.
  for (size_t i = 0; i < kVp9NumRefFrames; ++i) {
    if (header.RefreshFlag(i))
      reference_buffers_[i] = new_buffer_state;
  }

  return true;
}
}  // namespace test
}  // namespace media
