// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VP9_TEMPORAL_LAYERS_H_
#define MEDIA_GPU_VAAPI_VP9_TEMPORAL_LAYERS_H_

#include <stdint.h>

#include <vector>

#include "media/filters/vp9_parser.h"

namespace media {
class VP9Picture;
struct Vp9Metadata;

// This class defines a set of fixed temporal layers configurations, for two and
// three layers. It keeps a state of the current temporal index and is used to
// update each frame with the correct encoder settings to realize the selected
// pattern. NOTE: this class doesn't support spatial layers yet.
// Temporal layers and spatial layers are described in
// https://tools.ietf.org/html/draft-ietf-payload-vp9-10#section-3.
class VP9TemporalLayers {
 public:
  struct FrameConfig;

  // This class doesn't support spatial layers, but we have to
  // take them into account in determining the maximum number of used reference
  // frames for inter frames. It is because the maximum supported spatial layers
  // is three and the number of slots in the vp9 reference frames pool is eight,
  // the number of available reference frames is 2 (= 8/3).
  constexpr static size_t kMinSupportedTemporalLayers = 2u;
  constexpr static size_t kMaxSupportedTemporalLayers = 3u;
  constexpr static size_t kMaxSpatialLayers = 3u;
  constexpr static size_t kMaxNumUsedReferenceFrames =
      kVp9NumRefFrames / kMaxSpatialLayers;
  static_assert(kMaxNumUsedReferenceFrames == 2u,
                "VP9TemporalLayers uses two reference frames");

  explicit VP9TemporalLayers(size_t num_temporal_layers);
  ~VP9TemporalLayers();

  static std::vector<uint8_t> GetFpsAllocation(size_t num_temporal_layers);

  // Sets |picture|'s used reference frames and |ref_frames_used| so that they
  // structure valid temporal layers. This also fills |picture|'s
  // |metadata_for_encoding|.
  void FillUsedRefFramesAndMetadata(
      VP9Picture* picture,
      std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used);

  size_t num_layers() const { return num_layers_; }

 private:
  // Useful functions to construct refresh flag and detect reference frames
  // from the flag.
  uint8_t RefreshFrameFlag(const FrameConfig& temporal_layers_config) const;
  void FillVp9MetadataForEncoding(Vp9Metadata* metadata,
                                  const FrameConfig& temporal_layers_config,
                                  bool has_reference) const;
  void UpdateRefFramesPatternIndex(const FrameConfig& temporal_layers_config);

  // Following variables are configured upon construction, containing the amount
  // of temporal layers, the associated temporal layers indices and the nature
  // (reference, update, both, none) of each frame in the temporal group,
  // respectively.
  const size_t num_layers_;
  const std::vector<FrameConfig> temporal_layers_reference_pattern_;

  // The used slots of the vp9 reference pool.
  const uint8_t pool_slots_[kMaxNumUsedReferenceFrames];

  // The current index into the |temporal_layers_reference_pattern_|.
  uint8_t pattern_index_;

  // The pattern index used for reference frames slots.
  uint8_t pattern_index_of_ref_frames_slots_[kMaxNumUsedReferenceFrames] = {};
};
}  // namespace media
#endif  // MEDIA_GPU_VAAPI_VP9_TEMPORAL_LAYERS_H_
