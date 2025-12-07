// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/gpu/svc_layers.h"

#include <array>
#include <variant>

#include "base/logging.h"

namespace media {

namespace {

constexpr static size_t kMaxNumUsedRefFramesEachSpatialLayer = 2;
static_assert(kMaxNumUsedRefFramesEachSpatialLayer == 2u,
              "SVCLayers uses two reference frames for each spatial layer");
constexpr static size_t kMaxNumUsedReferenceFrames =
    kMaxNumUsedRefFramesEachSpatialLayer * SVCLayers::kMaxSpatialLayers;
static_assert(kMaxNumUsedReferenceFrames == 6u,
              "SVCLayers uses six reference frames");

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

  // SVCLayers uses 2 reference frame slots for each spatial layer, and
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
  const std::array<FrameFlags, kMaxNumUsedRefFramesEachSpatialLayer>
      buffer_flags_;
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
      constexpr auto TL2Pattern = std::to_array<FrameConfig>({
          FrameConfig(0, kReferenceAndUpdate, kNone, true),
          FrameConfig(1, kReference, kNone, true),
      });
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
      constexpr auto TL3Pattern = std::to_array<FrameConfig>({
          FrameConfig(0, kReferenceAndUpdate, kNone, true),
          FrameConfig(2, kReference, kNone, true),
          FrameConfig(1, kReference, kUpdate, true),
          FrameConfig(2, kNone, kReference, false),
      });
      return TL3Pattern[frame_num % std::size(TL3Pattern)];
    }
    default:
      NOTREACHED();
  }
}

// Checks if all the bitrate values in the active layers range are not zero and
// all the ones in non active layers range are zero.
bool ValidateBitrates(const VideoBitrateAllocation& bitrate_allocation,
                      size_t begin_active_spatial_layer,
                      size_t end_active_spatial_layer,
                      size_t num_temporal_layers) {
  for (size_t sid = 0; sid < VideoBitrateAllocation::kMaxSpatialLayers; ++sid) {
    for (size_t tid = 0; tid < VideoBitrateAllocation::kMaxTemporalLayers;
         ++tid) {
      const bool is_active = bitrate_allocation.GetBitrateBps(sid, tid) > 0;
      const bool expected_active = begin_active_spatial_layer <= sid &&
                                   sid < end_active_spatial_layer &&
                                   tid < num_temporal_layers;
      if (is_active != expected_active) {
        DVLOG(1) << "Invalid bitrate, sid=" << sid << ", tid=" << tid
                 << " : bitrate_allocation=" << bitrate_allocation.ToString();
        return false;
      }
    }
  }

  return true;
}

// Fills the spatial layers range and the number of temporal layers whose
// bitrate is not zero.
// |begin_active_spatial_layer| - the lowest active spatial layer index.
// |end_active_spatial_layer| - the last active spatial layer index + 1.
// |num_temporal_layers| - the number of temporal layers.
//
// The active spatial layer doesn't have to start with the bottom one, but the
// active temporal layer must start with the bottom one. In other words, if
// the spatial layer, spatial_index, is active, then
// GetBitrateBps(spatial_index, 0) must not be zero.
// Returns false VideoBitrateAllocation is invalid.
bool ValidateAndGetActiveLayers(
    const VideoBitrateAllocation& bitrate_allocation,
    size_t& begin_active_spatial_layer,
    size_t& end_active_spatial_layer,
    size_t& num_temporal_layers) {
  if (bitrate_allocation.GetSumBps() == 0) {
    DVLOG(1) << "No active bitrate: bitrate_allocation="
             << bitrate_allocation.ToString();
    return false;
  }

  begin_active_spatial_layer = 0;
  end_active_spatial_layer = 0;
  num_temporal_layers = 0;

  for (size_t sid = 0; sid < VideoBitrateAllocation::kMaxSpatialLayers; ++sid) {
    if (bitrate_allocation.GetBitrateBps(sid, 0) != 0) {
      begin_active_spatial_layer = sid;
      break;
    }
  }
  for (int sid = VideoBitrateAllocation::kMaxSpatialLayers - 1;
       sid >= base::checked_cast<int>(begin_active_spatial_layer); --sid) {
    if (bitrate_allocation.GetBitrateBps(sid, 0) != 0) {
      end_active_spatial_layer = sid + 1;
      break;
    }
  }

  if (end_active_spatial_layer == 0) {
    DVLOG(1) << "Invalid bitrate: bitrate_allocation="
             << bitrate_allocation.ToString();
    return false;
  }

  // This assumes the number of temporal layers are the same in all the spatial
  // layers. This will not be satisfied if we support a mix of hw/sw encoders.
  // See the discussion:
  // https://chromium-review.googlesource.com/c/chromium/src/+/5040171/2/media/base/video_bitrate_allocation.cc#200
  for (int tid = VideoBitrateAllocation::kMaxTemporalLayers - 1; tid >= 0;
       --tid) {
    if (bitrate_allocation.GetBitrateBps(begin_active_spatial_layer, tid) !=
        0) {
      num_temporal_layers = tid + 1;
      break;
    }
  }

  return ValidateBitrates(bitrate_allocation, begin_active_spatial_layer,
                          end_active_spatial_layer, num_temporal_layers);
}

}  // namespace

SVCLayers::Config::Config(
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

SVCLayers::Config::~Config() = default;
SVCLayers::Config::Config(const Config&) = default;
SVCLayers::PictureParam::PictureParam() = default;
SVCLayers::PictureParam::~PictureParam() = default;
SVCLayers::PictureParam::PictureParam(const PictureParam&) = default;

SVCLayers::SVCLayers(const Config& config) : config_(config) {}

std::pair<bool, std::optional<std::unique_ptr<SVCLayers>>>
SVCLayers::RecreateSVCLayersIfNeeded(
    VideoBitrateAllocation& bitrate_allocation) {
  size_t begin_active_spatial_layer;
  size_t end_active_spatial_layer;
  size_t num_temporal_layers;
  if (!ValidateAndGetActiveLayers(
          bitrate_allocation, begin_active_spatial_layer,
          end_active_spatial_layer, num_temporal_layers)) {
    // Invalid active layer.
    // See ValidateAndGetActiveLayers() comment for detail.
    return std::make_pair(false, std::nullopt);
  }

  const auto& old_config = config();
  if (end_active_spatial_layer > old_config.spatial_layer_resolutions.size() ||
      end_active_spatial_layer - begin_active_spatial_layer >
          old_config.spatial_layer_resolutions.size()) {
    DVLOG(1) << "Requested spatial layer exceeds the initial spatial layer "
             << "configuration: " << bitrate_allocation.ToString();
    return std::make_pair(false, std::nullopt);
  }

  // Change VideoBitrateAllocation so that the active spatial layers to
  // start with 0. This is necessary for the software rate controller.
  if (begin_active_spatial_layer > 0) {
    for (size_t sid = begin_active_spatial_layer;
         sid < end_active_spatial_layer; sid++) {
      for (size_t tid = 0; tid < num_temporal_layers; tid++) {
        const uint32_t bitrate = bitrate_allocation.GetBitrateBps(sid, tid);
        CHECK_NE(bitrate, 0u);
        bitrate_allocation.SetBitrate(sid - begin_active_spatial_layer, tid,
                                      bitrate);
        bitrate_allocation.SetBitrate(sid, tid, 0u);
      }
    }
  }

  // Only updating the number of temporal layers don't have to force keyframe.
  // But we produce keyframe in the case to not complex the code, assuming
  // updating the number of temporal layers don't often happen.
  // If this is not true, we should avoid producing keyframe in this case.
  if (old_config.begin_active_layer != begin_active_spatial_layer ||
      old_config.end_active_layer != end_active_spatial_layer ||
      old_config.num_temporal_layers != num_temporal_layers) {
    std::optional<std::unique_ptr<SVCLayers>> svc_layers =
        std::make_unique<SVCLayers>(SVCLayers::Config(
            old_config.spatial_layer_resolutions, begin_active_spatial_layer,
            end_active_spatial_layer, num_temporal_layers,
            old_config.inter_layer_pred));
    return std::make_pair(true, std::move(svc_layers));
  }

  return std::make_pair(true, std::nullopt);
}

void SVCLayers::Reset() {
  CHECK_EQ(spatial_idx_, 0u);
  frame_num_ = 0;
  frame_num_ref_frames_.fill(0);
}

void SVCLayers::PostEncode(uint8_t refresh_frame_flags) {
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

bool SVCLayers::IsKeyFrame() const {
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

void SVCLayers::GetPictureParamAndMetadata(
    PictureParam& picture_param,
    std::variant<Vp9Metadata*, SVCGenericMetadata*> metadata) const {
  picture_param.frame_size =
      config_.active_spatial_layer_resolutions[spatial_idx_];

  // |SVCLayers| follows the WebRTC SVC spec. so we don't use
  // |svc_metadata.reference_flags| and |svc_metadata.refresh_flags|.
  if (auto* svc_metadata = std::get_if<SVCGenericMetadata*>(&metadata)) {
    (*svc_metadata)->follow_svc_spec = true;
  }

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

void SVCLayers::FillMetadataForFirstFrame(
    std::variant<Vp9Metadata*, SVCGenericMetadata*> metadata,
    bool& key_frame,
    uint8_t& refresh_frame_flags,
    std::vector<uint8_t>& reference_frame_indices) const {
  CHECK_EQ(frame_num_, 0u);

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

  if (auto* svc_metadata = std::get_if<SVCGenericMetadata*>(&metadata)) {
    (*svc_metadata)->temporal_idx = 0;
    (*svc_metadata)->spatial_idx = spatial_idx_;
  } else {
    CHECK(std::holds_alternative<Vp9Metadata*>(metadata));
    auto& vp9_metadata = std::get<Vp9Metadata*>(metadata);
    // Since this is the first frame, there is no reference frame in the same
    // spatial layer.
    vp9_metadata->inter_pic_predicted = false;
    // The first frame is TL0 and references no frame.
    vp9_metadata->temporal_up_switch = true;

    vp9_metadata->end_of_picture =
        spatial_idx_ == config_.active_spatial_layer_resolutions.size() - 1;

    if (config_.inter_layer_pred == SVCInterLayerPredMode::kOnKeyPic) {
      vp9_metadata->referenced_by_upper_spatial_layers =
          !vp9_metadata->end_of_picture;
      vp9_metadata->reference_lower_spatial_layers = spatial_idx_ != 0;
    } else {
      vp9_metadata->referenced_by_upper_spatial_layers = false;
      vp9_metadata->reference_lower_spatial_layers = false;
    }

    vp9_metadata->temporal_idx = 0;
    vp9_metadata->spatial_idx = spatial_idx_;

    if (key_frame) {
      vp9_metadata->spatial_layer_resolutions =
          config_.active_spatial_layer_resolutions;
      vp9_metadata->begin_active_spatial_layer_index =
          base::checked_cast<uint8_t>(config_.begin_active_layer);
      vp9_metadata->end_active_spatial_layer_index =
          base::checked_cast<uint8_t>(config_.end_active_layer);
    }
  }
}

void SVCLayers::FillMetadataForNonFirstFrame(
    std::variant<Vp9Metadata*, SVCGenericMetadata*> metadata,
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

  if (auto* svc_metadata = std::get_if<SVCGenericMetadata*>(&metadata)) {
    (*svc_metadata)->temporal_idx = frame_config.layer_index();
    (*svc_metadata)->spatial_idx = spatial_idx_;
  } else {
    CHECK(std::holds_alternative<Vp9Metadata*>(metadata));
    auto& vp9_metadata = std::get<Vp9Metadata*>(metadata);
    vp9_metadata->inter_pic_predicted = !reference_frame_indices.empty();
    vp9_metadata->temporal_up_switch = frame_config.temporal_up_switch();

    // No reference between spatial layers in kOnKeyPic (frame_num!=0) and kOff.
    vp9_metadata->referenced_by_upper_spatial_layers = false;
    vp9_metadata->reference_lower_spatial_layers = false;

    vp9_metadata->end_of_picture =
        spatial_idx_ == config_.active_spatial_layer_resolutions.size() - 1;

    vp9_metadata->temporal_idx = frame_config.layer_index();
    vp9_metadata->spatial_idx = spatial_idx_;

    for (const uint8_t i : reference_frame_indices) {
      const uint8_t p_diff =
          base::checked_cast<uint8_t>(frame_num_ - frame_num_ref_frames_[i]);
      vp9_metadata->p_diffs.push_back(p_diff);
    }
  }
}
}  // namespace media
