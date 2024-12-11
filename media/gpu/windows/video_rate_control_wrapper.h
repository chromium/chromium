// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_VIDEO_RATE_CONTROL_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_VIDEO_RATE_CONTROL_WRAPPER_H_

#include <cstdint>
#include <memory>

#include "media/base/media_log.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

// These constants come from svc and codec spec.
constexpr size_t kMaxTemporalLayers = 8;
constexpr size_t kMaxSpatialLayers = 4;
constexpr size_t kMaxLayers = kMaxTemporalLayers * kMaxSpatialLayers;

// VideoRateControlWrapper is a base class for computing
// proper quantization param for each frame.
class VideoRateControlWrapper {
 public:
  // RateControlConfig is a type of helper for passing configs
  // to codec-specific rate controller.
  struct RateControlConfig {
    // Frame size.
    int width;
    int height;
    // Quantizer parameter，the range is 0-63.
    int max_quantizer;
    int min_quantizer;
    // Target_bandwidth is in kbps.
    int64_t target_bandwidth;
    // Frame rate.
    double framerate;
    // Content type, camera or display.
    VideoEncodeAccelerator::Config::ContentType content_type;
    // Target bitrate for svc layers.
    int layer_target_bitrate[kMaxLayers];
    // Rate decimator for temporal layers.
    int ts_rate_decimator[kMaxTemporalLayers];
    // Number of spatial layers.
    int ss_number_layers;
    // Number of temporal layers.
    int ts_number_layers;
    // Quantizer parameter for svc layers.
    int max_quantizers[kMaxLayers];
    int min_quantizers[kMaxLayers];
    // Scaling factor parameters for spatial layers.
    int scaling_factor_num[kMaxSpatialLayers];
    int scaling_factor_den[kMaxSpatialLayers];
  };

  // FrameParams is used for passing frame params.
  struct FrameParams {
    enum class FrameType { kKeyFrame, kInterFrame };
    FrameType frame_type;
    int spatial_layer_id;
    int temporal_layer_id;
    unsigned int timestamp;
  };

  virtual ~VideoRateControlWrapper() = default;
  virtual void UpdateRateControl(const RateControlConfig& config) = 0;
  // ComputeQP() returns qp table index and the range is up to the codec.
  virtual int ComputeQP(const FrameParams& frame_params) = 0;
  // GetLoopfilterLevel() is only available for VP9, others return -1.
  virtual int GetLoopfilterLevel() const = 0;
  // Feedback to rate control with the size of current encoded frame.
  virtual void PostEncodeUpdate(uint64_t encoded_frame_size,
                                const FrameParams& frame_params) = 0;
};

// VideoRateControlWrapperInternal is an interface for creating
// codec-specific rate controller.
template <typename RateControlConfigType,
          typename RateCtrlType,
          typename FrameParamsType>
class VideoRateControlWrapperInternal : public VideoRateControlWrapper {
 public:
  // Creates VideoRateControlWrapper implementation.
  static std::unique_ptr<VideoRateControlWrapperInternal> Create(
      const RateControlConfig& config) {
    auto impl = RateCtrlType::Create(ConvertControlConfig(config));
    if (!impl) {
      DLOG(ERROR) << "Failed creating video RateController";
      return nullptr;
    }
    return std::make_unique<VideoRateControlWrapperInternal>(std::move(impl));
  }
  VideoRateControlWrapperInternal() = default;
  explicit VideoRateControlWrapperInternal(std::unique_ptr<RateCtrlType> impl)
      : impl_(std::move(impl)) {}
  ~VideoRateControlWrapperInternal() override = default;
  void UpdateRateControl(const RateControlConfig& config) override {
    DCHECK(impl_);
    impl_->UpdateRateControl(ConvertControlConfig(config));
  }
  int ComputeQP(const FrameParams& frame_params) override {
    DCHECK(impl_);
    impl_->ComputeQP(ConvertFrameParams(frame_params));
    return impl_->GetQP();
  }
  int GetLoopfilterLevel() const override;
  void PostEncodeUpdate(uint64_t encoded_frame_size,
                        const FrameParams& frame_params) override;

 private:
  // "ConvertControlConfig" and "ConvertFrameParams" are used for passing
  // parameters to impl_, which should be specialized when the template is
  // instantiated.
  static RateControlConfigType ConvertControlConfig(
      const RateControlConfig& config);
  static FrameParamsType ConvertFrameParams(const FrameParams& frame_params);

  std::unique_ptr<RateCtrlType> impl_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_VIDEO_RATE_CONTROL_WRAPPER_H_
