// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VP9_SVC_LAYERS_H_
#define MEDIA_GPU_VP9_SVC_LAYERS_H_

#include <stdint.h>

#include <vector>

#include "media/filters/vp9_parser.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"

namespace media {
class VideoBitrateAllocation;
class VP9Picture;
struct Vp9Metadata;

// This class manages a state of K-SVC encoding up to three spatial and temporal
// layers. This supports activating/deactivating spatial layers and changing the
// number of temporal layers. The temporal layer sizes among spatial layers must
// be identical. Temporal layers and spatial layers are described in
// https://tools.ietf.org/html/draft-ietf-payload-vp9-10#section-3.
class MEDIA_GPU_EXPORT VP9SVCLayers {
 public:
  struct FrameConfig;

  constexpr static size_t kMaxSupportedTemporalLayers = 3u;
  constexpr static size_t kMaxSpatialLayers = 3u;
  constexpr static size_t kMaxNumUsedRefFramesEachSpatialLayer =
      kVp9NumRefFrames / kMaxSpatialLayers;
  static_assert(
      kMaxNumUsedRefFramesEachSpatialLayer == 2u,
      "VP9SVCLayers uses two reference frames for each spatial layer");
  constexpr static size_t kMaxNumUsedReferenceFrames =
      kMaxNumUsedRefFramesEachSpatialLayer * kMaxSpatialLayers;
  static_assert(kMaxNumUsedReferenceFrames == 6u,
                "VP9SVCLayers uses six reference frames");

  using SpatialLayer = VideoEncodeAccelerator::Config::SpatialLayer;
  explicit VP9SVCLayers(const std::vector<SpatialLayer>& spatial_layers,
                        SVCInterLayerPredMode inter_layer_pred);
  ~VP9SVCLayers();

  // Returns true if EncodeJob needs to produce key frame.
  bool UpdateEncodeJob(bool is_key_frame_requested, size_t kf_period_frames);

  // Activate/Deactivate spatial layers via |bitrate_allocation|.
  // Returns whether (de)updating is successful.
  bool MaybeUpdateActiveLayer(VideoBitrateAllocation* bitrate_allocation);

  // Sets |picture|'s used reference frames and |ref_frames_used| so that they
  // structure valid temporal layers. This also fills |picture|'s
  // |metadata_for_encoding|.
  void FillUsedRefFramesAndMetadata(
      VP9Picture* picture,
      std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used);

  size_t num_temporal_layers() const { return num_temporal_layers_; }
  const std::vector<gfx::Size>& active_spatial_layer_resolutions() const {
    return active_spatial_layer_resolutions_;
  }

 private:
  friend class VP9SVCLayersTest;

  // Useful functions to construct refresh flag and detect reference frames
  // from the flag.
  void FillVp9MetadataForEncoding(
      Vp9Metadata* metadata,
      const std::vector<uint8_t>& reference_frame_indices) const;
  void UpdateRefFramesPatternIndex(
      const std::vector<uint8_t>& refresh_frame_indices);

  // The variables of handling temporal layers structure.
  size_t num_temporal_layers_;
  std::vector<FrameConfig> temporal_layers_reference_pattern_;
  size_t temporal_pattern_size_;
  uint8_t pattern_index_ = 0;

  size_t spatial_idx_ = 0;
  size_t frame_num_ = 0;
  bool force_key_frame_ = false;

  // Resolutions for all spatial layers and active spatial layers.
  std::vector<gfx::Size> spatial_layer_resolutions_;
  std::vector<gfx::Size> active_spatial_layer_resolutions_;
  size_t begin_active_layer_;
  size_t end_active_layer_;

  // The pattern index used for reference frames slots.
  uint8_t pattern_index_of_ref_frames_slots_[kMaxNumUsedReferenceFrames] = {};

  // Inter layer prediction mode.
  const SVCInterLayerPredMode inter_layer_pred_;
};

}  // namespace media
#endif  // MEDIA_GPU_VP9_SVC_LAYERS_H_
