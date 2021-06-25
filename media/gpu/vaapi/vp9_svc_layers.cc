// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp9_svc_layers.h"

#include <bitset>

#include "base/logging.h"
#include "media/gpu/macros.h"
#include "media/gpu/vp9_picture.h"

namespace media {
namespace {
static_assert(VideoBitrateAllocation::kMaxTemporalLayers >=
                  VP9SVCLayers::kMaxSupportedTemporalLayers,
              "VP9SVCLayers and VideoBitrateAllocation are dimensionally "
              "inconsistent.");
static_assert(VideoBitrateAllocation::kMaxSpatialLayers >=
                  VP9SVCLayers::kMaxSpatialLayers,
              "VP9SVCLayers and VideoBitrateAllocation are dimensionally "
              "inconsistent.");

enum FrameFlags : uint8_t {
  kNone = 0,
  kReference = 1,
  kUpdate = 2,
  kReferenceAndUpdate = kReference | kUpdate,
};
}  // namespace

struct VP9SVCLayers::FrameConfig {
  FrameConfig(size_t layer_index, FrameFlags first, FrameFlags second)
      : layer_index_(layer_index), buffer_flags_{first, second} {}
  FrameConfig() = delete;

  // VP9SVCLayers uses 2 reference frames for each spatial layer, and totally
  // uses up to 6 reference frames. SL0 uses the first two (0, 1) reference
  // frames, SL1 uses middle two (2, 3) reference frames, and SL2 used last two
  // (4, 5) reference frames.
  std::vector<uint8_t> GetRefFrameIndices(size_t spatial_idx,
                                          size_t frame_num) const {
    std::vector<uint8_t> indices;
    if (frame_num != 0) {
      for (size_t i = 0; i < kMaxNumUsedRefFramesEachSpatialLayer; ++i) {
        if (buffer_flags_[i] & FrameFlags::kReference) {
          indices.push_back(i +
                            kMaxNumUsedRefFramesEachSpatialLayer * spatial_idx);
        }
      }
    } else {
      // For the key picture (|frame_num| equals 0), the higher spatial layer
      // reference the lower spatial layers. e.g. for frame_num 0, SL1 will
      // reference SL0, and SL2 will reference SL1.
      DCHECK_GT(spatial_idx, 0u);
      indices.push_back((spatial_idx - 1) *
                        kMaxNumUsedRefFramesEachSpatialLayer);
    }
    return indices;
  }
  std::vector<uint8_t> GetUpdateIndices(size_t spatial_idx) const {
    std::vector<uint8_t> indices;
    for (size_t i = 0; i < kMaxNumUsedRefFramesEachSpatialLayer; ++i) {
      if (buffer_flags_[i] & FrameFlags::kUpdate) {
        indices.push_back(i +
                          kMaxNumUsedRefFramesEachSpatialLayer * spatial_idx);
      }
    }
    return indices;
  }

  size_t layer_index() const { return layer_index_; }

 private:
  const size_t layer_index_;
  const FrameFlags buffer_flags_[kMaxNumUsedRefFramesEachSpatialLayer];
};

namespace {
// GetTemporalLayersReferencePattern() constructs the
// following temporal layers.
// 2 temporal layers structure: https://imgur.com/vBvHtdp.
// 3 temporal layers structure: https://imgur.com/pURAGvp.
std::vector<VP9SVCLayers::FrameConfig> GetTemporalLayersReferencePattern(
    size_t num_temporal_layers) {
  using FrameConfig = VP9SVCLayers::FrameConfig;
  // In a vp9 software encoder used in libwebrtc, each frame has only one
  // reference to the TL0 frame. It improves the encoding speed without reducing
  // the frame quality noticeably. This class, at this moment, lets each frame
  // have as many references as possible for the sake of better quality,
  // assuming a hardware encoder is sufficiently fast. TODO(crbug.com/1030199):
  // Measure speed vs. quality changing these structures.
  switch (num_temporal_layers) {
    case 1:
      // In this case, the number of spatial layers must great than 1.
      // TL0 references and updates the 'first' buffer.
      return {FrameConfig(0, kReferenceAndUpdate, kNone)};
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
std::vector<uint8_t> VP9SVCLayers::GetFpsAllocation(
    size_t num_temporal_layers) {
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
    case 1:
      // In this case, the number of spatial layers must great than 1.
      return {kFullAllocation};
    case 2:
      return {kFullAllocation / 2, kFullAllocation};
    case 3:
      return {kFullAllocation / 4, kFullAllocation / 2, kFullAllocation};
    default:
      NOTREACHED() << "Unsupported temporal layers";
      return {};
  }
}

VP9SVCLayers::VP9SVCLayers(const std::vector<SpatialLayer>& spatial_layers)
    : num_temporal_layers_(spatial_layers[0].num_of_temporal_layers),
      temporal_layers_reference_pattern_(
          GetTemporalLayersReferencePattern(num_temporal_layers_)),
      pattern_index_(0u),
      temporal_pattern_size_(temporal_layers_reference_pattern_.size()) {
  for (const auto spatial_layer : spatial_layers) {
    spatial_layer_resolutions_.emplace_back(
        gfx::Size(spatial_layer.width, spatial_layer.height));
  }
  active_spatial_layer_resolutions_ = spatial_layer_resolutions_;
  DCHECK_LE(num_temporal_layers_, kMaxSupportedTemporalLayers);
  DCHECK(!spatial_layer_resolutions_.empty());
  DCHECK_LE(spatial_layer_resolutions_.size(), kMaxSpatialLayers);
}

VP9SVCLayers::~VP9SVCLayers() = default;

bool VP9SVCLayers::UpdateEncodeJob(bool is_key_frame_requested,
                                   size_t kf_period_frames) {
  if (is_key_frame_requested) {
    frame_num_ = 0;
    spatial_idx_ = 0;
  }

  if (spatial_idx_ == active_spatial_layer_resolutions_.size()) {
    frame_num_++;
    frame_num_ %= kf_period_frames;
    spatial_idx_ = 0;
  }

  return frame_num_ == 0 && spatial_idx_ == 0;
}

void VP9SVCLayers::FillUsedRefFramesAndMetadata(
    VP9Picture* picture,
    std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used) {
  DCHECK(picture->frame_hdr);

  // Initialize |metadata_for_encoding| with default values.
  picture->metadata_for_encoding.emplace();
  ref_frames_used->fill(false);
  if (picture->frame_hdr->IsKeyframe()) {
    DCHECK_EQ(spatial_idx_, 0u);
    DCHECK_EQ(frame_num_, 0u);
    picture->frame_hdr->refresh_frame_flags = 0xff;

    // Start the pattern over from 0 and reset the buffer refresh states.
    pattern_index_ = 0;
    // For key frame, its temporal_layers_config is (0, kUpdate, kUpdate), so
    // its reference_frame_indices is empty, and refresh_frame_indices is {0, 1}
    FillVp9MetadataForEncoding(&(*picture->metadata_for_encoding),
                               /*reference_frame_indices=*/{});
    UpdateRefFramesPatternIndex(/*refresh_frame_indices=*/{0, 1});

    DVLOGF(4)
        << "Frame num: " << frame_num_
        << ", key frame: " << picture->frame_hdr->IsKeyframe()
        << ", spatial_idx: " << spatial_idx_ << ", temporal_idx: "
        << temporal_layers_reference_pattern_[pattern_index_].layer_index()
        << ", pattern index: " << static_cast<int>(pattern_index_)
        << ", refresh_frame_flags: "
        << std::bitset<8>(picture->frame_hdr->refresh_frame_flags);

    spatial_idx_++;
    return;
  }

  if (spatial_idx_ == 0)
    pattern_index_ = (pattern_index_ + 1) % temporal_pattern_size_;
  const VP9SVCLayers::FrameConfig& temporal_layers_config =
      temporal_layers_reference_pattern_[pattern_index_];

  // Set the slots in reference frame pool that will be updated.
  const std::vector<uint8_t> refresh_frame_indices =
      temporal_layers_config.GetUpdateIndices(spatial_idx_);
  for (const uint8_t i : refresh_frame_indices)
    picture->frame_hdr->refresh_frame_flags |= 1u << i;
  // Set the slots of reference frames used for the current frame.
  const std::vector<uint8_t> reference_frame_indices =
      temporal_layers_config.GetRefFrameIndices(spatial_idx_, frame_num_);

  uint8_t ref_flags = 0;
  for (size_t i = 0; i < reference_frame_indices.size(); i++) {
    (*ref_frames_used)[i] = true;
    picture->frame_hdr->ref_frame_idx[i] = reference_frame_indices[i];
    ref_flags |= 1 << reference_frame_indices[i];
  }

  DVLOGF(4) << "Frame num: " << frame_num_
            << ", key frame: " << picture->frame_hdr->IsKeyframe()
            << ", spatial_idx: " << spatial_idx_ << ", temporal_idx: "
            << temporal_layers_reference_pattern_[pattern_index_].layer_index()
            << ", pattern index: " << static_cast<int>(pattern_index_)
            << ", refresh_frame_flags: "
            << std::bitset<8>(picture->frame_hdr->refresh_frame_flags)
            << " reference buffers: " << std::bitset<8>(ref_flags);

  FillVp9MetadataForEncoding(&(*picture->metadata_for_encoding),
                             reference_frame_indices);
  UpdateRefFramesPatternIndex(refresh_frame_indices);
  spatial_idx_++;
}

void VP9SVCLayers::FillVp9MetadataForEncoding(
    Vp9Metadata* metadata,
    const std::vector<uint8_t>& reference_frame_indices) const {
  metadata->end_of_picture =
      spatial_idx_ == active_spatial_layer_resolutions_.size() - 1;
  metadata->referenced_by_upper_spatial_layers =
      frame_num_ == 0 &&
      spatial_idx_ < active_spatial_layer_resolutions_.size() - 1;

  // |spatial_layer_resolutions| has to be filled if and only if keyframe or the
  // number of active spatial layers is changed. However, we fill in the case of
  // keyframe, this works because if the number of active spatial layers is
  // changed, keyframe is requested.
  if (frame_num_ == 0 && spatial_idx_ == 0) {
    metadata->spatial_layer_resolutions = active_spatial_layer_resolutions_;
    return;
  }

  // Below parameters only needed to filled for non key frame.
  uint8_t temp_temporal_layers_id =
      temporal_layers_reference_pattern_[pattern_index_ %
                                         temporal_pattern_size_]
          .layer_index();
  // If |frame_num_| is zero, it refers only lower spatial layer.
  // |has_reference| is true if a frame in the same spatial layer is referred.
  if (frame_num_ != 0)
    metadata->has_reference = !reference_frame_indices.empty();
  metadata->temporal_up_switch = true;
  metadata->reference_lower_spatial_layers =
      frame_num_ == 0 && (spatial_idx_ != 0);
  metadata->temporal_idx = temp_temporal_layers_id;
  metadata->spatial_idx = spatial_idx_;

  for (const uint8_t i : reference_frame_indices) {
    // If |frame_num_| is zero, it refers only lower spatial layer, there is no
    // need to fill |p_diff|.
    if (frame_num_ != 0) {
      uint8_t p_diff = (pattern_index_ - pattern_index_of_ref_frames_slots_[i] +
                        temporal_pattern_size_) %
                       temporal_pattern_size_;
      // For non-key picture, its |p_diff| must large than 0.
      if (p_diff == 0)
        p_diff = temporal_pattern_size_;
      metadata->p_diffs.push_back(p_diff);
    }

    const uint8_t ref_temporal_layers_id =
        temporal_layers_reference_pattern_
            [pattern_index_of_ref_frames_slots_[i] % temporal_pattern_size_]
                .layer_index();
    metadata->temporal_up_switch &=
        (ref_temporal_layers_id != temp_temporal_layers_id);
  }
}

// Use current pattern index to update the reference frame's pattern index,
// this is used to calculate |p_diffs|.
void VP9SVCLayers::UpdateRefFramesPatternIndex(
    const std::vector<uint8_t>& refresh_frame_indices) {
  for (const uint8_t i : refresh_frame_indices)
    pattern_index_of_ref_frames_slots_[i] = pattern_index_;
}

}  // namespace media
