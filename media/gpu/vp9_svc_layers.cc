// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vp9_svc_layers.h"

#include "base/logging.h"

namespace media {

namespace {

constexpr static size_t kMaxNumUsedRefFramesEachSpatialLayer =
    kVp9NumRefFrames / VP9SVCLayers::kMaxSpatialLayers;
static_assert(kMaxNumUsedRefFramesEachSpatialLayer == 2u,
              "VP9SVCLayers uses two reference frames for each spatial layer");
constexpr static size_t kMaxNumUsedReferenceFrames =
    kMaxNumUsedRefFramesEachSpatialLayer * VP9SVCLayers::kMaxSpatialLayers;
static_assert(kMaxNumUsedReferenceFrames == 6u,
              "VP9SVCLayers uses six reference frames");

enum FrameFlags : uint8_t {
  kNone = 0,
  kReference = 1,
  kUpdate = 2,
  kReferenceAndUpdate = kReference | kUpdate,
};

struct FrameConfig {
  constexpr FrameConfig(size_t layer_index,
                        FrameFlags first,
                        FrameFlags second,
                        bool temporal_up_switch)
      : layer_index_(layer_index),
        buffer_flags_{first, second},
        temporal_up_switch_(temporal_up_switch) {}

  // VP9SVCLayers uses 2 reference frame slots for each spatial layer, and
  // totally uses up to 6 reference frame slots. SL0 uses the first two (0, 1)
  // slots, SL1 uses middle two (2, 3) slots, and SL2 uses last two (4, 5)
  // slots.
  std::vector<uint8_t> GetRefFrameIndices(size_t spatial_idx) const {
    std::vector<uint8_t> indices;
    for (size_t i = 0; i < kMaxNumUsedRefFramesEachSpatialLayer; ++i) {
      if (buffer_flags_[i] & FrameFlags::kReference) {
        indices.push_back(i +
                          kMaxNumUsedRefFramesEachSpatialLayer * spatial_idx);
      }
    }
    return indices;
  }

  std::vector<uint8_t> GetRefreshIndices(size_t spatial_idx) const {
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

FrameConfig GetFrameConfig(size_t num_temporal_layers, size_t frame_num) {
  switch (num_temporal_layers) {
    case 1:
      // In this case, the number of spatial layers must great than 1.
      // TL0 references and updates the 'first' buffer.
      // [TL0]---[TL0]
      return FrameConfig(0, kReferenceAndUpdate, kNone, true);
    case 2: {
      // TL0 references and updates the 'first' buffer.
      // TL1 references 'first' buffer.
      //      [TL1]
      //     /
      // [TL0]-----[TL0]
      constexpr FrameConfig TL2Pattern[] = {
          FrameConfig(0, kReferenceAndUpdate, kNone, true),
          FrameConfig(1, kReference, kNone, true),
      };
      return TL2Pattern[frame_num % std::size(TL2Pattern)];
    }
    case 3: {
      // TL0 references and updates the 'first' buffer.
      // TL1 references 'first' and updates 'second'.
      // TL2 references either 'first' or 'second' buffer.
      //    [TL2]      [TL2]
      //    _/   [TL1]--/
      //   /_______/
      // [TL0]--------------[TL0]
      constexpr FrameConfig TL3Pattern[] = {
          FrameConfig(0, kReferenceAndUpdate, kNone, true),
          FrameConfig(2, kReference, kNone, true),
          FrameConfig(1, kReference, kUpdate, true),
          FrameConfig(2, kNone, kReference, false),
      };
      return TL3Pattern[frame_num % std::size(TL3Pattern)];
    }
    default:
      NOTREACHED();
  }
}
}  // namespace

VP9SVCLayers::Config::Config(
    const std::vector<gfx::Size>& spatial_layer_resolutions,
    size_t begin_active_layer,
    size_t end_active_layer,
    size_t num_temporal_layers,
    SVCInterLayerPredMode inter_layer_pred)
    : spatial_layer_resolutions(spatial_layer_resolutions),
      begin_active_layer(begin_active_layer),
      end_active_layer(end_active_layer),
      num_temporal_layers(num_temporal_layers),
      active_spatial_layer_resolutions(
          spatial_layer_resolutions.begin() + begin_active_layer,
          spatial_layer_resolutions.begin() + end_active_layer),
      inter_layer_pred(inter_layer_pred) {}

VP9SVCLayers::Config::~Config() = default;
VP9SVCLayers::Config::Config(const Config&) = default;
VP9SVCLayers::PictureParam::PictureParam() = default;
VP9SVCLayers::PictureParam::~PictureParam() = default;
VP9SVCLayers::PictureParam::PictureParam(const PictureParam&) = default;

VP9SVCLayers::VP9SVCLayers(const Config& config) : config_(config) {}

void VP9SVCLayers::Reset() {
  CHECK_EQ(spatial_idx_, 0u);
  frame_num_ = 0;
  frame_num_ref_frames_.fill(0);
}

void VP9SVCLayers::PostEncode(uint8_t refresh_frame_flags) {
  for (size_t i = 0; i < kVp9NumRefFrames; ++i) {
    if (refresh_frame_flags & (1 << i)) {
      frame_num_ref_frames_[i] = frame_num_;
    }
  }

  spatial_idx_ += 1;
  if (spatial_idx_ == config_.active_spatial_layer_resolutions.size()) {
    spatial_idx_ = 0;
    frame_num_ += 1;
  }
}

bool VP9SVCLayers::IsKeyFrame() const {
  if (frame_num_ != 0) {
    return false;
  }
  if (config_.inter_layer_pred == SVCInterLayerPredMode::kOnKeyPic) {
    return spatial_idx_ == 0;
  }

  CHECK(config_.active_spatial_layer_resolutions.size() == 1 ||
        config_.inter_layer_pred == SVCInterLayerPredMode::kOff);
  return true;
}

void VP9SVCLayers::GetPictureParamAndMetadata(PictureParam& picture_param,
                                              Vp9Metadata& metadata) const {
  picture_param.frame_size =
      config_.active_spatial_layer_resolutions[spatial_idx_];

  if (frame_num_ == 0) {
    FillMetadataForFirstFrame(metadata, picture_param.key_frame,
                              picture_param.refresh_frame_flags,
                              picture_param.reference_frame_indices);
    return;
  }

  picture_param.key_frame = false;
  FillMetadataForNonFirstFrame(metadata, picture_param.refresh_frame_flags,
                               picture_param.reference_frame_indices);
}

void VP9SVCLayers::FillMetadataForFirstFrame(
    Vp9Metadata& metadata,
    bool& key_frame,
    uint8_t& refresh_frame_flags,
    std::vector<uint8_t>& reference_frame_indices) const {
  CHECK_EQ(frame_num_, 0u);

  // Since this is the first frame, there is no reference frame in the same
  // spatial layer.
  metadata.inter_pic_predicted = false;
  // The first frame is TL0 and references no frame.
  metadata.temporal_up_switch = true;

  metadata.end_of_picture =
      spatial_idx_ == config_.active_spatial_layer_resolutions.size() - 1;

  if (config_.inter_layer_pred == SVCInterLayerPredMode::kOnKeyPic) {
    metadata.referenced_by_upper_spatial_layers = !metadata.end_of_picture;
    metadata.reference_lower_spatial_layers = spatial_idx_ != 0;
  } else {
    metadata.referenced_by_upper_spatial_layers = false;
    metadata.reference_lower_spatial_layers = false;
  }

  metadata.temporal_idx = 0;
  metadata.spatial_idx = spatial_idx_;

  // Taking L3Tx as example, |refresh_indices| and |reference_frame_indices| are
  // as follows.
  // kOnKeyPic       | refresh_indices          | reference_frame_indices |
  //   L0 (keyframe) | {0, 1, 2, 3, 4, 5, 6, 7} | {}                      |
  //   L1            | {2}                      | {0}                     |
  //   L2            | {4}                      | {2}                     |
  //
  // KOff
  //   L0 (keyframe) | {0, 1, 2, 3, 4, 5, 6, 7} | {}                      |
  //   L1 (keyframe) | {2}                      | {}                      |
  //   L2 (keyframe) | {4}                      | {}                      |
  if (spatial_idx_ == 0) {
    key_frame = true;
    refresh_frame_flags = 0xff;
    reference_frame_indices = {};
  } else {
    key_frame = config_.inter_layer_pred == SVCInterLayerPredMode::kOff;
    refresh_frame_flags =
        1 << (spatial_idx_ * kMaxNumUsedRefFramesEachSpatialLayer);
    reference_frame_indices = {};
    if (config_.inter_layer_pred == SVCInterLayerPredMode::kOnKeyPic) {
      reference_frame_indices = {base::checked_cast<uint8_t>(
          (spatial_idx_ - 1) * kMaxNumUsedRefFramesEachSpatialLayer)};
    }
  }

  if (key_frame) {
    metadata.spatial_layer_resolutions =
        config_.active_spatial_layer_resolutions;
    metadata.begin_active_spatial_layer_index =
        base::checked_cast<uint8_t>(config_.begin_active_layer);
    metadata.end_active_spatial_layer_index =
        base::checked_cast<uint8_t>(config_.end_active_layer);
  }
}

void VP9SVCLayers::FillMetadataForNonFirstFrame(
    Vp9Metadata& metadata,
    uint8_t& refresh_frame_flags,
    std::vector<uint8_t>& reference_frame_indices) const {
  CHECK_NE(frame_num_, 0u);

  const FrameConfig frame_config =
      GetFrameConfig(config_.num_temporal_layers, frame_num_);
  refresh_frame_flags = 0;
  for (const uint8_t i : frame_config.GetRefreshIndices(spatial_idx_)) {
    refresh_frame_flags |= 1 << i;
  }

  reference_frame_indices = frame_config.GetRefFrameIndices(spatial_idx_);

  metadata.inter_pic_predicted = !reference_frame_indices.empty();
  metadata.temporal_up_switch = frame_config.temporal_up_switch();

  // No reference between spatial layers in kOnKeyPic (frame_num!=0) and kOff.
  metadata.referenced_by_upper_spatial_layers = false;
  metadata.reference_lower_spatial_layers = false;

  metadata.end_of_picture =
      spatial_idx_ == config_.active_spatial_layer_resolutions.size() - 1;

  metadata.temporal_idx = frame_config.layer_index();
  metadata.spatial_idx = spatial_idx_;

  for (const uint8_t i : reference_frame_indices) {
    const uint8_t p_diff =
        base::checked_cast<uint8_t>(frame_num_ - frame_num_ref_frames_[i]);
    metadata.p_diffs.push_back(p_diff);
  }
}
}  // namespace media
