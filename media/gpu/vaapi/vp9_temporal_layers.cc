// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp9_temporal_layers.h"

#include "media/gpu/vp9_picture.h"
#include "media/video/video_encode_accelerator.h"

namespace media {
namespace {
static_assert(VideoBitrateAllocation::kMaxTemporalLayers >=
                  VP9TemporalLayers::kMaxSupportedTemporalLayers,
              "VP9TemporalLayers and VideoBitrateAllocation are dimensionally "
              "inconsistent.");
static_assert(VideoBitrateAllocation::kMaxSpatialLayers >=
                  VP9TemporalLayers::kMaxSpatialLayers,
              "VP9TemporalLayers and VideoBitrateAllocation are dimensionally "
              "inconsistent.");

enum FrameFlags : uint8_t {
  kNone = 0,
  kReference = 1,
  kUpdate = 2,
  kReferenceAndUpdate = kReference | kUpdate,
};
}  // namespace

struct VP9TemporalLayers::FrameConfig {
  static constexpr size_t kNumFrameFlags =
      VP9TemporalLayers::kMaxNumUsedReferenceFrames;
  static_assert(kNumFrameFlags == 2u,
                "FrameConfig is only defined for carrying 2 FrameFlags");

  FrameConfig(size_t layer_index, FrameFlags first, FrameFlags second)
      : layer_index_(layer_index), buffer_flags_{first, second} {}
  FrameConfig() = delete;

  std::vector<uint8_t> GetReferenceIndices() const {
    std::vector<uint8_t> indices;
    for (size_t i = 0; i < FrameConfig::kNumFrameFlags; ++i) {
      if (buffer_flags_[i] & FrameFlags::kReference)
        indices.push_back(i);
    }
    return indices;
  }
  std::vector<uint8_t> GetUpdateIndices() const {
    std::vector<uint8_t> indices;
    for (size_t i = 0; i < FrameConfig::kNumFrameFlags; ++i) {
      if (buffer_flags_[i] & FrameFlags::kUpdate)
        indices.push_back(i);
    }
    return indices;
  }

  size_t layer_index() const { return layer_index_; }

 private:
  const size_t layer_index_;
  const FrameFlags buffer_flags_[kNumFrameFlags];
};

namespace {
// GetTemporalLayersReferencePattern() constructs the
// following temporal layers.
// 2 temporal layers structure: https://imgur.com/vBvHtdp.
// 3 temporal layers structure: https://imgur.com/pURAGvp.
constexpr size_t kTemporalLayersReferencePatternSize = 8;
std::vector<VP9TemporalLayers::FrameConfig> GetTemporalLayersReferencePattern(
    size_t num_layers) {
  using FrameConfig = VP9TemporalLayers::FrameConfig;
  // In a vp9 software encoder used in libwebrtc, each frame has only one
  // reference to the TL0 frame. It improves the encoding speed without reducing
  // the frame quality noticeably. This class, at this moment, lets each frame
  // have as many references as possible for the sake of better quality,
  // assuming a hardware encoder is sufficiently fast. TODO(crbug.com/1030199):
  // Measure speed vs. quality changing these structures.
  switch (num_layers) {
    case 2:
      // TL0 references and updates the 'first' buffer.
      // TL1 references 'first' and references and updates 'second'.
      return {FrameConfig(0, kReferenceAndUpdate, kNone),
              FrameConfig(1, kReference, kUpdate),
              FrameConfig(0, kReferenceAndUpdate, kNone),
              FrameConfig(1, kReference, kReferenceAndUpdate),
              FrameConfig(0, kReferenceAndUpdate, kNone),
              FrameConfig(1, kReference, kReferenceAndUpdate),
              FrameConfig(0, kReferenceAndUpdate, kNone),
              FrameConfig(1, kReference, kReferenceAndUpdate)};
    case 3:
      // TL0 references and updates the 'first' buffer.
      // TL1 references 'first' and references and updates 'second'.
      // TL2 references, if there are, both 'first' and 'second' but updates no
      // buffer.
      return {FrameConfig(0, kReferenceAndUpdate, kNone),
              FrameConfig(2, kReference, kNone),
              FrameConfig(1, kReference, kUpdate),
              FrameConfig(2, kReference, kReference),
              FrameConfig(0, kReferenceAndUpdate, kNone),
              FrameConfig(2, kReference, kReference),
              FrameConfig(1, kReference, kReferenceAndUpdate),
              FrameConfig(2, kReference, kReference)};
    default:
      NOTREACHED();
      return {};
  }
}
}  // namespace

// static
std::vector<uint8_t> VP9TemporalLayers::GetFpsAllocation(
    size_t num_temporal_layers) {
  DCHECK_GT(num_temporal_layers, 1u);
  DCHECK_LT(num_temporal_layers, 4u);
  constexpr uint8_t kFullAllocation = 255;
  // The frame rate fraction is given as an 8 bit unsigned integer where 0 = 0%
  // and 255 = 100%. Each layer's allocated fps refers to the previous one, so
  // e.g. your camera is opened at 30fps, and you want to have decode targets at
  // 15fps and 7.5fps as well:
  // TL0 then gets an allocation of 7.5/30 = 1/4. TL1 adds another 7.5fps to end
  // up at (7.5 + 7.5)/30 = 15/30 = 1/2 of the total allocation. TL2 adds the
  // final 15fps to end up at (15 + 15)/30, which is the full allocation.
  // Therefor, fps_allocation values are as follows,
  // fps_allocation[0][0] = kFullAllocation / 4;
  // fps_allocation[0][1] = kFullAllocation / 2;
  // fps_allocation[0][2] = kFullAllocation;
  //  For more information, see webrtc::VideoEncoderInfo::fps_allocation.
  switch (num_temporal_layers) {
    case 2:
      return {kFullAllocation / 2, kFullAllocation};
    case 3:
      return {kFullAllocation / 4, kFullAllocation / 2, kFullAllocation};
    default:
      NOTREACHED() << "Unsupported temporal layers";
      return {};
  }
}

VP9TemporalLayers::VP9TemporalLayers(size_t number_of_temporal_layers)
    : num_layers_(number_of_temporal_layers),
      temporal_layers_reference_pattern_(
          GetTemporalLayersReferencePattern(num_layers_)),
      pool_slots_{0, 1},
      pattern_index_(0u) {
  DCHECK_LE(kMinSupportedTemporalLayers, number_of_temporal_layers);
  DCHECK_LE(number_of_temporal_layers, kMaxSupportedTemporalLayers);
  DCHECK_EQ(temporal_layers_reference_pattern_.size(),
            kTemporalLayersReferencePatternSize);
}

VP9TemporalLayers::~VP9TemporalLayers() = default;

void VP9TemporalLayers::FillUsedRefFramesAndMetadata(
    VP9Picture* picture,
    std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used) {
  DCHECK(picture->frame_hdr);

  // Initialize |metadata_for_encoding| with default values.
  picture->metadata_for_encoding.emplace();
  ref_frames_used->fill(false);
  if (picture->frame_hdr->IsKeyframe()) {
    picture->frame_hdr->refresh_frame_flags = 0xff;

    // Start the pattern over from 0 and reset the buffer refresh states.
    pattern_index_ = 0;
    UpdateRefFramesPatternIndex(FrameConfig(0, kUpdate, kUpdate));
    return;
  }

  pattern_index_ = (pattern_index_ + 1) % kTemporalLayersReferencePatternSize;
  const VP9TemporalLayers::FrameConfig& temporal_layers_config =
      temporal_layers_reference_pattern_[pattern_index_];

  // Set the slots in reference frame pool that will be updated.
  picture->frame_hdr->refresh_frame_flags =
      RefreshFrameFlag(temporal_layers_config);
  // Set the slots of reference frames used for the current frame.
  std::vector<uint8_t> ref_frame_pool_indices;
  for (const uint8_t i : temporal_layers_config.GetReferenceIndices())
    ref_frame_pool_indices.push_back(pool_slots_[i]);
  for (size_t i = 0; i < ref_frame_pool_indices.size(); i++) {
    (*ref_frames_used)[i] = true;
    picture->frame_hdr->ref_frame_idx[i] = ref_frame_pool_indices[i];
  }

  FillVp9MetadataForEncoding(&(*picture->metadata_for_encoding),
                             temporal_layers_config,
                             !ref_frame_pool_indices.empty());
  UpdateRefFramesPatternIndex(temporal_layers_config);
}

void VP9TemporalLayers::FillVp9MetadataForEncoding(
    Vp9Metadata* metadata,
    const VP9TemporalLayers::FrameConfig& temporal_layers_config,
    bool has_reference) const {
  uint8_t temp_temporal_layers_id =
      temporal_layers_reference_pattern_[pattern_index_ %
                                         kTemporalLayersReferencePatternSize]
          .layer_index();
  metadata->has_reference = has_reference;
  metadata->temporal_idx = temp_temporal_layers_id;

  metadata->temporal_up_switch = true;
  for (const uint8_t i : temporal_layers_config.GetReferenceIndices()) {
    metadata->p_diffs.push_back((pattern_index_ -
                                 pattern_index_of_ref_frames_slots_[i] +
                                 kTemporalLayersReferencePatternSize) %
                                kTemporalLayersReferencePatternSize);

    const uint8_t ref_temporal_layers_id =
        temporal_layers_reference_pattern_
            [pattern_index_of_ref_frames_slots_[i] %
             kTemporalLayersReferencePatternSize]
                .layer_index();
    metadata->temporal_up_switch &=
        (ref_temporal_layers_id != temp_temporal_layers_id);
  }
}

uint8_t VP9TemporalLayers::RefreshFrameFlag(
    const VP9TemporalLayers::FrameConfig& temporal_layers_config) const {
  uint8_t flag = 0;
  for (const uint8_t i : temporal_layers_config.GetUpdateIndices())
    flag |= 1 << pool_slots_[i];
  return flag;
}

void VP9TemporalLayers::UpdateRefFramesPatternIndex(
    const VP9TemporalLayers::FrameConfig& temporal_layers_config) {
  for (const uint8_t i : temporal_layers_config.GetUpdateIndices())
    pattern_index_of_ref_frames_slots_[i] = pattern_index_;
}
}  // namespace media
