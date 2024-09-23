// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
#include "media/gpu/test/video_encoder/decoder_buffer_validator.h"

#include <set>
#include <vector>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "build/buildflag.h"
#include "media/base/decoder_buffer.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/macros.h"
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

// static
std::unique_ptr<DecoderBufferValidator> DecoderBufferValidator::Create(
    VideoCodecProfile profile,
    const gfx::Rect& visible_rect,
    size_t num_spatial_layers,
    size_t num_temporal_layers,
    SVCInterLayerPredMode inter_layer_pred) {
  CHECK_LE(num_spatial_layers, kMaxSpatialLayers);
  CHECK_LE(num_temporal_layers, kMaxSpatialLayers);
  switch (VideoCodecProfileToVideoCodec(profile)) {
    case VideoCodec::kH264:
      return std::make_unique<H264Validator>(profile, visible_rect,
                                             num_temporal_layers);
    case VideoCodec::kVP8:
      return std::make_unique<VP8Validator>(visible_rect, num_temporal_layers);
    case VideoCodec::kVP9:
      // Only SVCInterLayerPredMode::kOnKeyPic (for VP9 k-SVC) and
      // SVCInterLayerPredMode::kOff (for VP9 S-mode) are supported.
      CHECK_NE(inter_layer_pred, SVCInterLayerPredMode::kOn);
      return std::make_unique<VP9Validator>(
          profile, visible_rect, num_spatial_layers, num_temporal_layers,
          inter_layer_pred);
    case VideoCodec::kAV1:
      return std::make_unique<AV1Validator>(visible_rect);
    default:
      LOG(ERROR) << "Unsupported profile: " << GetProfileName(profile);
      return nullptr;
  }
}

DecoderBufferValidator::DecoderBufferValidator(const gfx::Rect& visible_rect,
                                               size_t num_temporal_layers)
    : visible_rect_(visible_rect), num_temporal_layers_(num_temporal_layers) {}

DecoderBufferValidator::~DecoderBufferValidator() = default;

void DecoderBufferValidator::ProcessBitstream(
    scoped_refptr<BitstreamRef> bitstream,
    size_t frame_index) {
  CHECK(bitstream);
  if (!Validate(bitstream->buffer.get(), bitstream->metadata)) {
    num_errors_++;
  }
}

bool DecoderBufferValidator::WaitUntilDone() {
  return num_errors_ == 0;
}

H264Validator::H264Validator(VideoCodecProfile profile,
                             const gfx::Rect& visible_rect,
                             size_t num_temporal_layers,
                             std::optional<uint8_t> level)
    : DecoderBufferValidator(visible_rect, num_temporal_layers),
      cur_pic_(new H264Picture),
      profile_(VideoCodecProfileToH264ProfileIDC(profile)),
      level_(level) {}

H264Validator::~H264Validator() = default;

bool H264Validator::Validate(const DecoderBuffer* buffer,
                             const BitstreamBufferMetadata& metadata) {
  if (metadata.dropped_frame()) {
    if (metadata.key_frame) {
      LOG(ERROR) << "Don't drop key frame";
      return false;
    }
    if (metadata.h264.has_value()) {
      LOG(ERROR) << "BitstreamBufferMetadata has H264Metadata on dropped frame";
      return false;
    }
    return true;
  }

  if (!metadata.end_of_picture()) {
    LOG(ERROR) << "end_of_picture must be true always in H264";
    return false;
  }

  CHECK(buffer);
  const DecoderBuffer& decoder_buffer = *buffer;
  parser_.SetStream(decoder_buffer.data(), decoder_buffer.size());

  if (num_temporal_layers_ > 1) {
    if (!metadata.h264) {
      LOG(ERROR) << "H264Metadata must be filled in temporal layer encoding";
      return false;
    }
    if (metadata.h264->temporal_idx >= num_temporal_layers_) {
      LOG(ERROR) << "Invalid temporal_idx: "
                 << base::strict_cast<int32_t>(metadata.h264->temporal_idx);
      return false;
    }
  }

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

        if (!metadata.key_frame) {
          LOG(ERROR) << "metadata.key_frame is false on IDR frame";
          return false;
        }

        if (metadata.h264 &&
            (metadata.h264->temporal_idx != 0 || metadata.h264->layer_sync)) {
          LOG(ERROR) << "temporal_idx="
                     << base::strict_cast<int>(metadata.h264->temporal_idx)
                     << " or layer_sync="
                     << base::strict_cast<int>(metadata.h264->layer_sync)
                     << " is unexpected";
          return false;
        }

        [[fallthrough]];
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

        CHECK(parser_.GetPPS(cur_pps_id_));
        const int qp = slice_hdr.slice_qp_delta +
                       parser_.GetPPS(cur_pps_id_)->pic_init_qp_minus26 + 26;
        DVLOGF(4) << "qp=" << qp;
        const int temporal_idx =
            metadata.h264 ? metadata.h264->temporal_idx : 0;
        qp_values_[0][temporal_idx].push_back(qp);
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

VP8Validator::VP8Validator(const gfx::Rect& visible_rect,
                           size_t num_temporal_layers)
    : DecoderBufferValidator(visible_rect, num_temporal_layers) {}

VP8Validator::~VP8Validator() = default;

bool VP8Validator::Validate(const DecoderBuffer* buffer,
                            const BitstreamBufferMetadata& metadata) {
  if (metadata.dropped_frame()) {
    if (metadata.key_frame) {
      LOG(ERROR) << "Don't drop key frame";
      return false;
    }
    if (metadata.vp8.has_value()) {
      LOG(ERROR) << "BitstreamBufferMetadata has Vp8Metadata on dropped frame";
      return false;
    }
    return true;
  }

  if (!metadata.end_of_picture()) {
    LOG(ERROR) << "end_of_picture must be true always in VP8";
    return false;
  }

  CHECK(buffer);
  const DecoderBuffer& decoder_buffer = *buffer;

  // TODO(hiroh): We could be getting more frames in the buffer, but there is
  // no simple way to detect this. We'd need to parse the frames and go through
  // partition numbers/sizes. For now assume one frame per buffer.
  Vp8FrameHeader header;
  if (!parser_.ParseFrame(decoder_buffer.data(), decoder_buffer.size(),
                          &header)) {
    LOG(ERROR) << "Failed parsing";
    return false;
  }

  const int qp = base::strict_cast<int>(header.quantization_hdr.y_ac_qi);
  DVLOGF(4) << "qp=" << qp;
  if (!header.show_frame) {
    LOG(ERROR) << "|show_frame| should be always true";
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

  if (!seen_keyframe_) {
    LOG(ERROR) << "Bitstream cannot start with a delta frame";
    return false;
  }

  if (num_temporal_layers_ == 1) {
    if (!header.refresh_entropy_probs) {
      LOG(ERROR) << "refereh_entropy_probs should be true in non temporal "
                    "layer encoding";
      return false;
    }
    qp_values_[0][0].push_back(qp);
    return true;
  } else if (header.refresh_entropy_probs) {
    LOG(ERROR)
        << "refereh_entropy_probs must be false in temporal layer encoding";
    return false;
  }

  if (!metadata.vp8) {
    LOG(ERROR) << "Metadata must be populated if temporal scalability is used.";
    return false;
  }

  const uint8_t temporal_idx = metadata.vp8->temporal_idx;
  if (temporal_idx >= num_temporal_layers_) {
    LOG(ERROR) << "Invalid temporal id: "
               << base::strict_cast<int>(temporal_idx);
    return false;
  }
  qp_values_[0][temporal_idx].push_back(qp);
  if (header.IsKeyframe()) {
    if (temporal_idx != 0) {
      LOG(ERROR) << "Temporal id must be 0 on keyframe";
      return false;
    }
    return true;
  }

  if (header.copy_buffer_to_golden != Vp8FrameHeader::NO_GOLDEN_REFRESH ||
      header.copy_buffer_to_alternate != Vp8FrameHeader::NO_ALT_REFRESH) {
    LOG(ERROR) << "Each reference frame is either updated by the current "
               << "frame or unchanged in temporal layer encoding.";
    return false;
  }

  const bool update_reference = header.refresh_last ||
                                header.refresh_golden_frame ||
                                header.refresh_alternate_frame;
  if (metadata.vp8->non_reference != !update_reference) {
    LOG(ERROR) << "|non_reference| must be true iff the frame does not update"
               << "any reference buffer.";
    return false;
  }

  if (num_temporal_layers_ == temporal_idx + 1) {
    if (update_reference) {
      LOG(ERROR) << "The frame in top temporal layer must not update "
                    "reference frame.";
      return false;
    }
  } else {
    // TL0 can update last frame only and TL1 can update golden frame only when
    // it is not top temporal layer.
    if (temporal_idx == 0) {
      if (!header.refresh_last || header.refresh_golden_frame ||
          header.refresh_alternate_frame) {
        LOG(ERROR) << "|refresh_last| only must be set on temporal layer 0";
        return false;
      }
    } else if (temporal_idx == 1) {
      if (header.refresh_last || !header.refresh_golden_frame ||
          header.refresh_alternate_frame) {
        LOG(ERROR) << "|refresh_golden_frame| only must be set on temporal "
                   << "layer 1";
        return false;
      }
    }
  }

  return true;
}

VP9Validator::VP9Validator(VideoCodecProfile profile,
                           const gfx::Rect& visible_rect,
                           size_t max_num_spatial_layers,
                           size_t num_temporal_layers,
                           SVCInterLayerPredMode inter_layer_pred)
    : DecoderBufferValidator(visible_rect, num_temporal_layers),
      profile_(VideoCodecProfileToVP9Profile(profile)),
      max_num_spatial_layers_(max_num_spatial_layers),
      s_mode_(max_num_spatial_layers > 1 &&
              inter_layer_pred == SVCInterLayerPredMode::kOff),
      cur_num_spatial_layers_(max_num_spatial_layers_),
      next_picture_id_(0) {
  const size_t num_parsed_streams = s_mode_ ? max_num_spatial_layers_ : 1u;
  for (size_t i = 0; i < num_parsed_streams; ++i) {
    parsers_.push_back(
        std::make_unique<Vp9Parser>(/*parsing_compressed_header=*/false));
  }
  reference_buffers_.resize(num_parsed_streams);
}

VP9Validator::~VP9Validator() = default;

bool VP9Validator::Validate(const DecoderBuffer* buffer,
                            const BitstreamBufferMetadata& metadata) {
  if (metadata.dropped_frame()) {
    if (metadata.key_frame) {
      LOG(ERROR) << "Don't drop key frame";
      return false;
    }
    if (metadata.vp9.has_value()) {
      LOG(ERROR)
          << "BitstreamBufferMetadata has Vp9Metadata on a dropped frame";
      return false;
    }
    if (metadata.end_of_picture()) {
      dropped_superframe_timestamp_.reset();
    } else {
      if (!dropped_superframe_timestamp_) {
        dropped_superframe_timestamp_ = metadata.timestamp;
      }
      if (*dropped_superframe_timestamp_ != metadata.timestamp) {
        LOG(ERROR) << "A timestamp mismatch on dropped frame in the same "
                   << "spatial layers";
        return false;
      }
    }
    return true;
  }

  if (dropped_superframe_timestamp_ &&
      *dropped_superframe_timestamp_ == metadata.timestamp) {
    LOG(ERROR) << "A frame on upper spatial layers are not dropped though a "
               << "frame on bottom spatial layers is dropped";
    return false;
  }

  CHECK(buffer);
  const DecoderBuffer& decoder_buffer = *buffer;

  // See Annex B "Superframes" in VP9 spec.
  constexpr uint8_t kSuperFrameMarkerMask = 0b11100000;
  constexpr uint8_t kSuperFrameMarker = 0b11000000;
  if ((base::span(decoder_buffer).back() & kSuperFrameMarkerMask) ==
      kSuperFrameMarker) {
    LOG(ERROR) << "Support for super-frames not yet implemented.";
    return false;
  }

  const bool svc_encoding =
      max_num_spatial_layers_ > 1 || num_temporal_layers_ > 1;
  if (metadata.vp9.has_value() != svc_encoding) {
    LOG(ERROR) << "VP9 specific metadata must exist only for SVC encodings";
    return false;
  }

  const size_t parser_index =
      s_mode_ ? begin_active_spatial_layer_index_ + metadata.vp9->spatial_idx
              : 0;

  CHECK_LT(parser_index, parsers_.size());
  auto& parser = *parsers_[parser_index];
  Vp9FrameHeader header;
  gfx::Size allocate_size;
  parser.SetStream(decoder_buffer.data(), decoder_buffer.size(), nullptr);
  if (parser.ParseNextFrame(&header, &allocate_size, nullptr) ==
      Vp9Parser::kInvalidStream) {
    LOG(ERROR) << "Failed parsing";
    return false;
  }

  if (metadata.key_frame != header.IsKeyframe()) {
    LOG(ERROR) << "Keyframe info in metadata is wrong, metadata.keyframe="
               << metadata.key_frame;
    return false;
  }
  if (header.profile != static_cast<uint8_t>(profile_)) {
    LOG(ERROR) << "Profile mismatched. Actual profile: "
               << static_cast<int>(header.profile)
               << ", expected profile: " << profile_;
    return false;
  }

  if (metadata.vp9.has_value() != svc_encoding) {
    LOG(ERROR) << "VP9 specific metadata must exist if and only if the stream "
               << "is temporal or spatial layer stream";
    return false;
  }

  if (s_mode_) {
    return ValidateSmodeStream(decoder_buffer, metadata, header);
  } else if (svc_encoding) {
    return ValidateSVCStream(decoder_buffer, metadata, header);
  }
  return ValidateVanillaStream(decoder_buffer, metadata, header);
}

bool VP9Validator::ValidateVanillaStream(
    const DecoderBuffer& decoder_buffer,
    const BitstreamBufferMetadata& metadata,
    const Vp9FrameHeader& header) {
  if (next_picture_id_ == 0 && !metadata.key_frame) {
    LOG(ERROR) << "First frame must be a keyframe.";
    return false;
  }
  if (header.error_resilient_mode) {
    LOG(ERROR) << "Error resilient mode should not be used if neither spatial"
                  "nor temporal scalablity is enabled";
    return false;
  }
  if (!header.refresh_frame_context) {
#if BUILDFLAG(USE_VAAPI)
    // TODO(b/297226972): Remove the workaround once the iHD driver is fixed.
    LOG(WARNING) << "Frame context should be refreshed if neither spatial nor "
                    "temporal scalablity is enabled";
#else
    LOG(ERROR) << "Frame context should be refreshed if neither spatial nor "
                  "temporal scalablity is enabled";
    return false;
#endif
  }

  if (metadata.key_frame) {
    next_picture_id_ = 0;
  }

  BufferState new_buffer_state{};
  new_buffer_state.picture_id = next_picture_id_++;

  if (header.show_existing_frame &&
      !reference_buffers_[0][header.frame_to_show_map_idx]) {
    LOG(ERROR) << "Attempting to show an existing frame, but the selected "
                  "reference buffer is invalid.";
    return false;
  }

  // Check the resolution is expected.
  const gfx::Rect visible_rect(header.render_width, header.render_height);
  if (visible_rect_ != visible_rect) {
    LOG(ERROR) << "Visible rectangle mismatched. Actual visible_rect: "
               << visible_rect.ToString()
               << ", expected visible_rect: " << visible_rect_.ToString();
    return false;
  }

  if (!header.IsIntra()) {
    for (uint8_t ref_frame_index : header.ref_frame_idx) {
      if (ref_frame_index >= static_cast<uint8_t>(kVp9NumRefFrames)) {
        LOG(ERROR) << "Invalid reference frame index: "
                   << static_cast<int>(ref_frame_index);
        return false;
      }
      if (!reference_buffers_[0][ref_frame_index]) {
        LOG(ERROR) << "Frame is trying to reference buffer with invalid state.";
        return false;
      }
    }
  }

  // Update current state with the new buffer.
  for (size_t i = 0; i < kVp9NumRefFrames; ++i) {
    if (header.RefreshFlag(i)) {
      reference_buffers_[0][i] = new_buffer_state;
    }
  }

  const int qp = base::strict_cast<int>(header.quant_params.base_q_idx);
  DVLOGF(4) << "qp=" << qp;
  qp_values_[0][0].push_back(qp);
  return true;
}

bool VP9Validator::ValidateSVCStream(const DecoderBuffer& decoder_buffer,
                                     const BitstreamBufferMetadata& metadata,
                                     const Vp9FrameHeader& header) {
  const Vp9Metadata& vp9 = *metadata.vp9;
  if (next_picture_id_ == 0 && vp9.spatial_idx == 0 && !metadata.key_frame) {
    LOG(ERROR) << "First frame must be a keyframe.";
    return false;
  }

  if (!header.error_resilient_mode) {
    LOG(ERROR) << "Error resilient mode must be used if spatial or temporal "
                  "scaliblity is enabled.";
    return false;
  }

  if (header.refresh_frame_context) {
    LOG(ERROR) << "Frame context must not be refreshed if spatial or temporal "
               << " scalability is enabled";
    return false;
  }

  if (vp9.spatial_idx >= cur_num_spatial_layers_ ||
      vp9.temporal_idx >= num_temporal_layers_) {
    LOG(ERROR) << "Invalid spatial_idx="
               << base::strict_cast<int>(vp9.spatial_idx)
               << ", temporal_idx=" << base::strict_cast<int>(vp9.temporal_idx);
    return false;
  }
  if (vp9.inter_pic_predicted != (vp9.p_diffs.size() > 0)) {
    LOG(ERROR) << "Inconsistent metadata, inter_pic_predicted implies p_diffs "
                  "is non-empty.";
    return false;
  }
  if (metadata.key_frame) {
    if (vp9.spatial_idx != 0 || vp9.temporal_idx != 0) {
      LOG(ERROR) << "Spatial and temporal id must be 0 for keyframes.";
      return false;
    }
    if (vp9.spatial_layer_resolutions.empty()) {
      LOG(ERROR) << "spatial_layer_resolution must not be empty on key frame";
      return false;
    }

    cur_num_spatial_layers_ = vp9.spatial_layer_resolutions.size();
    spatial_layer_resolutions_ = vp9.spatial_layer_resolutions;
    next_picture_id_ = 0;
  } else if (header.show_existing_frame) {
    if (!reference_buffers_[0][header.frame_to_show_map_idx]) {
      LOG(ERROR) << "Attempting to show an existing frame, but the selected "
                    "reference buffer is invalid.";
      return false;
    }
    int expected_diff =
        next_picture_id_ -
        reference_buffers_[0][header.frame_to_show_map_idx]->picture_id;
    if (vp9.p_diffs.size() != 1 || vp9.p_diffs[0] != expected_diff) {
      LOG(ERROR) << "Inconsistency between p_diff and existing frame to show.";
      return false;
    }
    return true;
  }

  BufferState new_buffer_state{
      .picture_id = next_picture_id_,
      .spatial_id = vp9.spatial_idx,
      .temporal_id = vp9.temporal_idx,
  };

  const bool end_of_picture = vp9.spatial_idx == cur_num_spatial_layers_ - 1;
  if (end_of_picture != metadata.end_of_picture()) {
    LOG(ERROR) << "end_of_picture mismatches: end_of_picture=" << end_of_picture
               << ", metadata.end_of_picture=" << metadata.end_of_picture();
    return false;
  }

  if (end_of_picture) {
    next_picture_id_++;
  }

  // Check the resolution is expected.
  const gfx::Rect visible_rect(header.render_width, header.render_height);
  if (visible_rect.size() != spatial_layer_resolutions_[vp9.spatial_idx]) {
    LOG(ERROR) << "Resolution mismatched. Actual resolution: "
               << visible_rect.size().ToString() << ", expected resolution: "
               << spatial_layer_resolutions_[vp9.spatial_idx].ToString();
    return false;
  }

  // Check that referenced frames are OK.
  if (header.IsIntra()) {
    if (!vp9.p_diffs.empty()) {
      // TODO(crbug.com/40172317): Consider if this is truly an error-state.
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
      if (!reference_buffers_[0][ref_frame_index]) {
        LOG(ERROR) << "Frame is trying to reference buffer with invalid state.";
        return false;
      }
      const BufferState& ref = *reference_buffers_[0][ref_frame_index];
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
      // |p_diffs| even though it references lower spatial layer frame. Skip
      // inserting |expected_pdiffs|.
      if (new_buffer_state.picture_id != 0) {
        expected_pdiffs.push_back(new_buffer_state.picture_id - ref.picture_id);
      }
    }
    for (uint8_t p_diff : vp9.p_diffs) {
      if (!std::erase(expected_pdiffs, p_diff)) {
        LOG(ERROR)
            << "Frame is referencing buffer not contained in the p_diff.";
        return false;
      }
    }
    if (!expected_pdiffs.empty()) {
      // TODO(crbug.com/40172317): Consider if this is truly an error-state.
      LOG(ERROR) << "|p_diff| contains frame that is not actually referenced.";
      return false;
    }
  }

  if (vp9.temporal_up_switch) {
    // Temporal up-switch, invalidate any buffers containing frames with higher
    // temporal id.
    for (auto& buffer : reference_buffers_[0]) {
      if (buffer && buffer->temporal_id > new_buffer_state.temporal_id) {
        buffer.reset();
      }
    }
  }

  // Update current state with the new buffer.
  for (size_t i = 0; i < kVp9NumRefFrames; ++i) {
    if (header.RefreshFlag(i)) {
      reference_buffers_[0][i] = new_buffer_state;
    }
  }

  const int qp = base::strict_cast<int>(header.quant_params.base_q_idx);
  DVLOGF(4) << "qp=" << qp;
  qp_values_[vp9.spatial_idx][vp9.temporal_idx].push_back(qp);
  return true;
}

bool VP9Validator::ValidateSmodeStream(const DecoderBuffer& decoder_buffer,
                                       const BitstreamBufferMetadata& metadata,
                                       const Vp9FrameHeader& header) {
  const Vp9Metadata& vp9 = *metadata.vp9;
  if (next_picture_id_ == 0 && !metadata.key_frame) {
    LOG(ERROR) << "First frame on each layer must be a keyframe.";
    return false;
  }
  if (!header.error_resilient_mode) {
    LOG(ERROR) << "Error resilient mode must be used in s-mode encoding";
    return false;
  }
  if (header.refresh_frame_context) {
    LOG(ERROR) << "Frame context must not be refreshed in s-mode encoding";
    return false;
  }
  if (vp9.spatial_idx >= cur_num_spatial_layers_ ||
      vp9.temporal_idx >= num_temporal_layers_) {
    LOG(ERROR) << "Invalid spatial_idx="
               << base::strict_cast<int>(vp9.spatial_idx)
               << ", temporal_idx=" << base::strict_cast<int>(vp9.temporal_idx);
    return false;
  }
  if (vp9.referenced_by_upper_spatial_layers) {
    LOG(ERROR) << "referenced_by_upper_spatial_layers must be always false in "
               << "s-mode encoding";
    return false;
  }
  if (vp9.inter_pic_predicted != (vp9.p_diffs.size() > 0)) {
    LOG(ERROR) << "Inconsistent metadata, inter_pic_predicted implies p_diffs "
                  "is non-empty.";
    return false;
  }

  if (metadata.key_frame) {
    if (vp9.temporal_idx != 0) {
      LOG(ERROR) << "Temporal id must be 0 for keyframes.";
      return false;
    }
    if (vp9.spatial_layer_resolutions.empty()) {
      LOG(ERROR) << "spatial_layer_resolution must not be empty on keyframe";
      return false;
    }
    cur_num_spatial_layers_ = vp9.spatial_layer_resolutions.size();
    spatial_layer_resolutions_ = vp9.spatial_layer_resolutions;
    next_picture_id_ = 0;
    begin_active_spatial_layer_index_ = vp9.begin_active_spatial_layer_index;
  } else if (header.show_existing_frame) {
    const size_t stream_index =
        vp9.spatial_idx + begin_active_spatial_layer_index_;
    if (!reference_buffers_[stream_index][header.frame_to_show_map_idx]) {
      LOG(ERROR) << "Attempting to show an existing frame, but the selected "
                    "reference buffer is invalid.";
      return false;
    }
    int expected_diff =
        next_picture_id_ -
        reference_buffers_[stream_index][header.frame_to_show_map_idx]
            ->picture_id;
    if (vp9.p_diffs.size() != 1 || vp9.p_diffs[0] != expected_diff) {
      LOG(ERROR) << "Inconsistency between p_diff and existing frame to show.";
      return false;
    }
    return true;
  }

  BufferState new_buffer_state{
      .picture_id = next_picture_id_,
      .temporal_id = vp9.temporal_idx,
  };

  const bool end_of_picture = vp9.spatial_idx == cur_num_spatial_layers_ - 1;
  if (end_of_picture != metadata.end_of_picture()) {
    LOG(ERROR) << "end_of_picture mismatches: end_of_picture=" << end_of_picture
               << ", metadata.end_of_picture=" << metadata.end_of_picture();
    return false;
  }
  if (end_of_picture) {
    next_picture_id_++;
  }

  // Check the resolution is expected.

  const gfx::Rect visible_rect(header.render_width, header.render_height);
  if (visible_rect.size() != spatial_layer_resolutions_[vp9.spatial_idx]) {
    LOG(ERROR) << "Resolution mismatched. Actual resolution: "
               << visible_rect.size().ToString() << ", expected resolution: "
               << spatial_layer_resolutions_[vp9.spatial_idx].ToString();
    return false;
  }
  // Check that referenced frames are OK.
  if (header.IsIntra()) {
    if (!vp9.p_diffs.empty()) {
      // TODO(crbug.com/40172317): Consider if this is truly an error-state.
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
      if (!reference_buffers_[vp9.spatial_idx][ref_frame_index]) {
        LOG(ERROR) << "Frame is trying to reference buffer with invalid state.";
        return false;
      }
      const BufferState& ref =
          *reference_buffers_[vp9.spatial_idx][ref_frame_index];
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
      expected_pdiffs.push_back(new_buffer_state.picture_id - ref.picture_id);
    }
    for (uint8_t p_diff : vp9.p_diffs) {
      if (!std::erase(expected_pdiffs, p_diff)) {
        LOG(ERROR)
            << "Frame is referencing buffer not contained in the p_diff.";
        return false;
      }
    }
    if (!expected_pdiffs.empty()) {
      // TODO(crbug.com/40172317): Consider if this is truly an error-state.
      LOG(ERROR) << "|p_diff| contains frame that is not actually referenced.";
      return false;
    }
  }

  if (vp9.temporal_up_switch) {
    // Temporal up-switch, invalidate any buffers containing frames with higher
    // temporal id.
    for (auto& buffer : reference_buffers_[vp9.spatial_idx]) {
      if (buffer && buffer->temporal_id > new_buffer_state.temporal_id) {
        buffer.reset();
      }
    }
  }

  // Update current state with the new buffer.
  for (size_t i = 0; i < kVp9NumRefFrames; ++i) {
    if (header.RefreshFlag(i))
      reference_buffers_[vp9.spatial_idx][i] = new_buffer_state;
  }

  const int qp = base::strict_cast<int>(header.quant_params.base_q_idx);
  DVLOGF(4) << "qp=" << qp;
  qp_values_[vp9.spatial_idx][vp9.temporal_idx].push_back(qp);
  return true;
}

AV1Validator::AV1Validator(const gfx::Rect& visible_rect)
    : DecoderBufferValidator(visible_rect, /*num_temporal_layers=*/1),
      buffer_pool_(libgav1::OnInternalFrameBufferSizeChanged,
                   libgav1::GetInternalFrameBuffer,
                   libgav1::ReleaseInternalFrameBuffer,
                   &buffer_list_) {}

// TODO(b/268487938): Add more robust testing here. Currently we only perform
// the most basic validation that the bitstream parses correctly and has the
// right dimensions.
bool AV1Validator::Validate(const DecoderBuffer* buffer,
                            const BitstreamBufferMetadata& metadata) {
  if (metadata.dropped_frame()) {
    if (metadata.key_frame) {
      LOG(ERROR) << "Don't drop key frame";
      return false;
    }
    if (metadata.av1.has_value()) {
      LOG(ERROR) << "BitstreamBufferMetadata has Av1Metadata on dropped frame";
      return false;
    }
    return true;
  }
  if (!metadata.end_of_picture()) {
    LOG(ERROR) << "end_of_picture must be true always in AV1";
    return false;
  }

  CHECK(buffer);
  const DecoderBuffer& decoder_buffer = *buffer;
  libgav1::ObuParser av1_parser(decoder_buffer.data(), decoder_buffer.size(), 0,
                                &buffer_pool_, &decoder_state_);
  libgav1::RefCountedBufferPtr curr_frame;

  if (sequence_header_) {
    av1_parser.set_sequence_header(*sequence_header_);
  }

  auto parse_status = av1_parser.ParseOneFrame(&curr_frame);
  if (parse_status != libgav1::kStatusOk) {
    LOG(ERROR) << "Failed parsing frame. Status: " << parse_status;
    return false;
  }

  const auto& frame_header = av1_parser.frame_header();
  if (gfx::Size(frame_header.render_width, frame_header.render_height) !=
      visible_rect_.size()) {
    LOG(ERROR) << "Mismatched visible rectangle dimensions.";
    LOG(ERROR) << "Got render_width=" << frame_header.render_width
               << " render_height=" << frame_header.render_height;
    LOG(ERROR) << "Expected visible_width=" << visible_rect_.width()
               << " visible_height=" << visible_rect_.height();
    return false;
  }

  if (frame_header.frame_type != libgav1::FrameType::kFrameKey &&
      frame_num_ == 0) {
    LOG(ERROR) << "First frame must be keyframe";
    return false;
  }

  if (frame_header.frame_type == libgav1::FrameType::kFrameKey) {
    frame_num_ = 0;
  }

  if (frame_header.order_hint != (frame_num_ & 0xFF)) {
    LOG(ERROR) << "Incorrect frame order hint";
    LOG(ERROR) << "Got: " << frame_header.order_hint;
    LOG(ERROR) << "Expected: " << (int)(frame_num_ & 0xFF);
    return false;
  }

  const int qp = base::strict_cast<int>(frame_header.quantizer.base_index);
  DVLOGF(4) << "qp=" << qp;
  qp_values_[0][0].push_back(qp);

  // Update our state for the next frame.
  if (av1_parser.frame_header().frame_type == libgav1::FrameType::kFrameKey) {
    sequence_header_ = av1_parser.sequence_header();
  }
  decoder_state_.UpdateReferenceFrames(
      curr_frame, av1_parser.frame_header().refresh_frame_flags);

  frame_num_++;

  return true;
}
}  // namespace test
}  // namespace media
