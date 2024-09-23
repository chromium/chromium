// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VP9_SVC_LAYERS_H_
#define MEDIA_GPU_VP9_SVC_LAYERS_H_

#include <stdint.h>

#include <vector>

#include "media/base/svc_scalability_mode.h"
#include "media/gpu/media_gpu_export.h"
#include "media/parsers/vp9_parser.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MEDIA_GPU_EXPORT VP9SVCLayers {
 public:
  constexpr static size_t kMaxTemporalLayers = 3u;
  constexpr static size_t kMaxSpatialLayers = 3u;

  // Config is the SVC configuration used in VP9SVCLayers. It cannot be changed
  // after creation. In other words, if active spatial layers or the number of
  // temporal layers are changed, a client needs to recreate VP9SVCLayers with
  // the new Config.
  struct MEDIA_GPU_EXPORT Config {
    Config(const std::vector<gfx::Size>& spatial_layer_resolutions,
           size_t begin_active_layer,
           size_t end_active_layer,
           size_t num_temporal_layers,
           SVCInterLayerPredMode inter_layer_pred);
    ~Config();
    Config(const Config& config);

    // The resolutions in spatial layers. This includes resolutions in inactive
    // resolutions.
    std::vector<gfx::Size> spatial_layer_resolutions;
    // The active spatial layer resolutions index.
    // [begin_active_layer, end_active_layer - 1].
    size_t begin_active_layer = 0;
    size_t end_active_layer = 0;
    size_t num_temporal_layers = 0;
    // The active spatial layer resolutions, i.e., resolutions from
    // spatial_layer_resolutions[begin_active_layer] to
    // spatial_layer_resolutions[end_active_layer - 1].
    std::vector<gfx::Size> active_spatial_layer_resolutions;
    SVCInterLayerPredMode inter_layer_pred = SVCInterLayerPredMode::kOnKeyPic;
  };

  // The parameters used for encoding the current frame.
  struct MEDIA_GPU_EXPORT PictureParam {
    PictureParam();
    ~PictureParam();
    PictureParam(const PictureParam&);

    bool key_frame = false;
    gfx::Size frame_size;
    uint8_t refresh_frame_flags = 0;
    std::vector<uint8_t> reference_frame_indices;
  };

  explicit VP9SVCLayers(const Config& config);

  // These functions are constant functions. Unless Reset() or PostEncode() is
  // called, these functions returns the same result as the previous result.
  // Returns whether the current frame shall encoded as key frame.
  bool IsKeyFrame() const;
  // Gets PictureParam and fill the metadata for the current frame.
  void GetPictureParamAndMetadata(PictureParam& picture_param,
                                  Vp9Metadata& metadata) const;

  // Resets the current spatial layer stream.
  // This can be called before encoding the SVC frame.
  void Reset();
  // Moves to the next frame by changing |spatial_index_| and |frame_num_| and
  // updates |frame_num_ref_frames_| by |refresh_frame_flags|.
  void PostEncode(uint8_t refresh_frame_flags);

  // Returns the currently used configuration and the spatial index and the
  // frame number for the current frame.
  const Config& config() const { return config_; }
  size_t spatial_idx() const { return spatial_idx_; }
  size_t frame_num() const { return frame_num_; }

 private:
  // Fill metadata for the first frame, i.e. frame_num=0.
  void FillMetadataForFirstFrame(
      Vp9Metadata& metadata,
      bool& key_frame,
      uint8_t& refresh_frame_flags,
      std::vector<uint8_t>& reference_frame_indices) const;
  // Fill metadata for the second and later frames, i.e. frame_num != 0.
  void FillMetadataForNonFirstFrame(
      Vp9Metadata& metadata,
      uint8_t& refresh_frame_flags,
      std::vector<uint8_t>& reference_frame_indices) const;

  const Config config_;

  size_t spatial_idx_ = 0;
  // The frame number since the last frame containing keyframe.
  // It is incremented after PostEncode() on the top spatial index.
  size_t frame_num_ = 0;
  // The |frame_num| of reference frames. This is used to compute |p_diff|.
  std::array<size_t, kVp9NumRefFrames> frame_num_ref_frames_;
};
}  // namespace media
#endif  // MEDIA_GPU_VP9_SVC_LAYERS_H_
