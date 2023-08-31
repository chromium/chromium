// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp9_svc_layers.h"

#include <bitset>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/macros.h"
#include "media/gpu/vp9_picture.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

const char* GetSVCName(size_t begin_active_layer,
                       size_t end_active_layer,
                       size_t num_temporal_layers) {
  DCHECK_LT(begin_active_layer, end_active_layer);
  size_t num_spatial_layers = end_active_layer - begin_active_layer;
  absl::optional<SVCScalabilityMode> svc_mode;
  switch (num_spatial_layers) {
    case 1:
      switch (num_temporal_layers) {
        case 1:
          return "L1T1";
        case 2:
          svc_mode = SVCScalabilityMode::kL1T2;
          break;
        case 3:
          svc_mode = SVCScalabilityMode::kL1T3;
          break;
      }
      break;
    case 2:
      switch (num_temporal_layers) {
        case 1:
          svc_mode = SVCScalabilityMode::kL2T1Key;
          break;
        case 2:
          svc_mode = SVCScalabilityMode::kL2T2Key;
          break;
        case 3:
          svc_mode = SVCScalabilityMode::kL2T3Key;
          break;
      }
      break;
    case 3:
      switch (num_temporal_layers) {
        case 1:
          svc_mode = SVCScalabilityMode::kL3T1Key;
          break;
        case 2:
          svc_mode = SVCScalabilityMode::kL3T2Key;
          break;
        case 3:
          svc_mode = SVCScalabilityMode::kL3T3Key;
          break;
      }
      break;
  }

  if (!svc_mode) {
    DVLOGF(1) << "Unexpected SVC config: "
              << "begin_active_layer=" << begin_active_layer
              << ", end_active_layer=" << end_active_layer
              << ", num_temporal_layers=" << num_temporal_layers;
    return "INVALID SVC";
  }
  return GetScalabilityModeName(*svc_mode);
}
}  // namespace

struct VP9SVCLayers::FrameConfig {
  FrameConfig(size_t layer_index,
              FrameFlags first,
              FrameFlags second,
              bool temporal_up_switch)
      : layer_index_(layer_index),
        buffer_flags_{first, second},
        temporal_up_switch_(temporal_up_switch) {}
  FrameConfig() = delete;

  // VP9SVCLayers uses 2 reference frame slots for each spatial layer, and
  // totally uses up to 6 reference frame slots. SL0 uses the first two (0, 1)
  // slots, SL1 uses middle two (2, 3) slots, and SL2 uses last two (4, 5)
  // slots.
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
  bool temporal_up_switch() const { return temporal_up_switch_; }

 private:
  const size_t layer_index_;
  const FrameFlags buffer_flags_[kMaxNumUsedRefFramesEachSpatialLayer];
  const bool temporal_up_switch_;
};

namespace {
// GetTemporalLayersReferencePattern() constructs the
// following temporal layers.
std::vector<VP9SVCLayers::FrameConfig> GetTemporalLayersReferencePattern(
    size_t num_temporal_layers) {
  using FrameConfig = VP9SVCLayers::FrameConfig;
  switch (num_temporal_layers) {
    case 1:
      // In this case, the number of spatial layers must great than 1.
      // TL0 references and updates the 'first' buffer.
      // [TL0]---[TL0]
      return {FrameConfig(0, kReferenceAndUpdate, kNone, true)};
    case 2:
      // TL0 references and updates the 'first' buffer.
      // TL1 references 'first' buffer.
      //      [TL1]
      //     /
      // [TL0]-----[TL0]
      return {FrameConfig(0, kReferenceAndUpdate, kNone, true),
              FrameConfig(1, kReference, kNone, true)};
    case 3:
      // TL0 references and updates the 'first' buffer.
      // TL1 references 'first' and updates 'second'.
      // TL2 references either 'first' or 'second' buffer.
      //    [TL2]      [TL2]
      //    _/   [TL1]--/
      //   /_______/
      // [TL0]--------------[TL0]
      return {FrameConfig(0, kReferenceAndUpdate, kNone, true),
              FrameConfig(2, kReference, kNone, true),
              FrameConfig(1, kReference, kUpdate, true),
              FrameConfig(2, kNone, kReference, true)};
    default:
      NOTREACHED_NORETURN();
  }
}
}  // namespace

VP9SVCLayers::VP9SVCLayers(const std::vector<SpatialLayer>& spatial_layers)
    : num_temporal_layers_(spatial_layers[0].num_of_temporal_layers),
      temporal_layers_reference_pattern_(
          GetTemporalLayersReferencePattern(num_temporal_layers_)),
      temporal_pattern_size_(temporal_layers_reference_pattern_.size()) {
  for (const auto spatial_layer : spatial_layers) {
    spatial_layer_resolutions_.emplace_back(
        gfx::Size(spatial_layer.width, spatial_layer.height));
  }
  active_spatial_layer_resolutions_ = spatial_layer_resolutions_;
  begin_active_layer_ = 0;
  end_active_layer_ = active_spatial_layer_resolutions_.size();
  CHECK_LE(num_temporal_layers_, kMaxSupportedTemporalLayers);
  CHECK(!spatial_layer_resolutions_.empty());
  CHECK_LE(spatial_layer_resolutions_.size(), kMaxSpatialLayers);
}

VP9SVCLayers::~VP9SVCLayers() = default;

bool VP9SVCLayers::UpdateEncodeJob(bool is_key_frame_requested,
                                   size_t kf_period_frames) {
  if (force_key_frame_ || is_key_frame_requested) {
    frame_num_ = 0;
    spatial_idx_ = 0;
    force_key_frame_ = false;
  }

  if (spatial_idx_ == active_spatial_layer_resolutions_.size()) {
    frame_num_++;
    frame_num_ %= kf_period_frames;
    spatial_idx_ = 0;
  }

  return frame_num_ == 0 && spatial_idx_ == 0;
}

bool VP9SVCLayers::MaybeUpdateActiveLayer(
    VideoBitrateAllocation* bitrate_allocation) {
  // Don't update active layer if current picture haven't completed SVC
  // encoding. Since the |spatial_idx_| is updated in the beginning of next
  // encoding, so the |spatial_idx_| equals 0 (only for the first frame) or the
  // number of active spatial layers indicates the complement of SVC picture
  // encoding.
  if (spatial_idx_ != 0 &&
      spatial_idx_ != active_spatial_layer_resolutions_.size()) {
    DVLOGF(1)
        << "MaybeUpdateActiveLayer() is called in non bottom spatial layer";
    return false;
  }

  size_t begin_active_layer = kMaxSpatialLayers;
  size_t end_active_layer = spatial_layer_resolutions_.size();
  size_t new_num_temporal_layers = 0;
  for (size_t sid = 0; sid < spatial_layer_resolutions_.size(); ++sid) {
    size_t num_active_temporal_layers_of_current_layer = 0;
    for (int tid = kMaxSupportedTemporalLayers - 1; tid >= 0; tid--) {
      const uint32_t tl_bitrate = bitrate_allocation->GetBitrateBps(sid, tid);
      if (tl_bitrate != 0) {
        num_active_temporal_layers_of_current_layer = tid + 1;
        break;
      }
    }

    // Now, |active_num_temporal_layers_| is zero if the current spatial layer
    // is deactivated, or the bitrates of the current spatial layer is [X_i]
    for (size_t tid = 0; tid < num_active_temporal_layers_of_current_layer;
         ++tid) {
      const uint32_t tl_bitrate = bitrate_allocation->GetBitrateBps(sid, tid);
      // A bitrate of a lower temporal layer than |active_num_temporal_layers|
      // must not be zero, e.g. {0, 0, 100}.
      if (tl_bitrate == 0) {
        DVLOGF(1) << "The bitrate of a lower temporal layer is zero while the "
                  << "bitrate of an upper temporal layer is non zero";
        return false;
      }
    }

    // Check if the temporal layers higher than |kMaxSupportedTemporalLayers|
    // are zero.
    for (size_t tid = kMaxSupportedTemporalLayers;
         tid < VideoBitrateAllocation::kMaxTemporalLayers; ++tid) {
      if (bitrate_allocation->GetBitrateBps(sid, tid) != 0u) {
        DVLOGF(1) << "Unsupported temporal layers";
        return false;
      }
    }

    if (num_active_temporal_layers_of_current_layer == 0) {
      // This is the first non-active spatial layer in the end side.
      if (begin_active_layer != kMaxSpatialLayers) {
        end_active_layer = sid;
        break;
      }
      // No active spatial layer is found yet. Try the upper spatial layer.
      continue;
    }

    // Check if the number of active temporal layers is same among spatial
    // layers.
    if (new_num_temporal_layers != 0 &&
        num_active_temporal_layers_of_current_layer !=
            new_num_temporal_layers) {
      DVLOGF(1) << "The invalid active temporal layers: "
                << "num_active_temporal_layers_of_current_layer="
                << num_active_temporal_layers_of_current_layer
                << ", new_num_temporal_layers=" << new_num_temporal_layers;
      return false;
    }

    // This is the lowest active layer.
    if (begin_active_layer == kMaxSpatialLayers) {
      begin_active_layer = sid;
      new_num_temporal_layers = num_active_temporal_layers_of_current_layer;
    }
  }
  // Check if all the bitrates of unsupported temporal and spatial layers are
  // zero.
  for (size_t sid = end_active_layer;
       sid < VideoBitrateAllocation::kMaxSpatialLayers; ++sid) {
    for (size_t tid = 0; tid < VideoBitrateAllocation::kMaxTemporalLayers;
         ++tid) {
      if (bitrate_allocation->GetBitrateBps(sid, tid) != 0u)
        return false;
    }
  }
  // No active layer is found.
  if (begin_active_layer == kMaxSpatialLayers) {
    DVLOGF(1) << "No active spatial layers";
    return false;
  }

  DCHECK_NE(new_num_temporal_layers, 0u);
  DCHECK_LT(begin_active_layer, end_active_layer);
  DCHECK_LE(end_active_layer - begin_active_layer,
            spatial_layer_resolutions_.size());

  // Remove non active spatial layer bitrate if |begin_active_layer| > 0.
  if (begin_active_layer > 0) {
    for (size_t sid = begin_active_layer; sid < end_active_layer; ++sid) {
      for (size_t tid = 0; tid < new_num_temporal_layers; ++tid) {
        uint32_t bitrate = bitrate_allocation->GetBitrateBps(sid, tid);
        bitrate_allocation->SetBitrate(sid - begin_active_layer, tid, bitrate);
        bitrate_allocation->SetBitrate(sid, tid, 0u);
      }
    }
  }

  // Reset SVC parameters and force to produce key frame if active layer
  // changed.
  if (begin_active_layer != begin_active_layer_ ||
      end_active_layer != end_active_layer_ ||
      new_num_temporal_layers != num_temporal_layers_) {
    DVLOGF(2) << "SVC structure is changed from "
              << GetSVCName(begin_active_layer_, end_active_layer_,
                            num_temporal_layers_)
              << " to "
              << GetSVCName(begin_active_layer, end_active_layer,
                            new_num_temporal_layers);

    // Update the stored active layer range.
    begin_active_layer_ = begin_active_layer;
    end_active_layer_ = end_active_layer;
    active_spatial_layer_resolutions_ = {
        spatial_layer_resolutions_.begin() + begin_active_layer,
        spatial_layer_resolutions_.begin() + end_active_layer};
    force_key_frame_ = true;

    // Only updating the number of temporal layers don't have to force keyframe.
    // But we produce keyframe in the case to not complex the code, assuming
    // updating the number of temporal layers don't often happen.
    // If this is not true, we should avoid producing keyframe in this case.
    num_temporal_layers_ = new_num_temporal_layers;
    temporal_layers_reference_pattern_ =
        GetTemporalLayersReferencePattern(num_temporal_layers_);
    temporal_pattern_size_ = temporal_layers_reference_pattern_.size();
    // |pattern_index_| is updated to zero in FillUsedRefFramesAndMetadata().
  }
  return true;
}

void VP9SVCLayers::FillUsedRefFramesAndMetadata(
    VP9Picture* picture,
    std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used) {
  DCHECK(picture->frame_hdr);
  // Update the spatial layer size for VP9FrameHeader.
  gfx::Size updated_size = active_spatial_layer_resolutions_[spatial_idx_];
  picture->frame_hdr->render_width = updated_size.width();
  picture->frame_hdr->render_height = updated_size.height();
  picture->frame_hdr->frame_width = updated_size.width();
  picture->frame_hdr->frame_height = updated_size.height();

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
    picture->metadata_for_encoding->temporal_up_switch = true;

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
  picture->metadata_for_encoding->temporal_up_switch =
      temporal_layers_config.temporal_up_switch();
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
    // |begin_active_layer_| and |end_active_layer_| are less than
    // |spatial_layer_resolutions_.size()|, which is
    // VP9SVCLayers::kMaxSpatialLayers at most.
    metadata->begin_active_spatial_layer_index =
        base::checked_cast<uint8_t>(begin_active_layer_);
    metadata->end_active_spatial_layer_index =
        base::checked_cast<uint8_t>(end_active_layer_);
    return;
  }

  // Below parameters only needed to filled for non key frame.
  uint8_t temp_temporal_layers_id =
      temporal_layers_reference_pattern_[pattern_index_ %
                                         temporal_pattern_size_]
          .layer_index();
  // If |frame_num_| is zero, it refers only lower spatial layer.
  // |inter_pic_predicted| is true if a frame in the same spatial layer is
  // referred.
  if (frame_num_ != 0)
    metadata->inter_pic_predicted = !reference_frame_indices.empty();
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
