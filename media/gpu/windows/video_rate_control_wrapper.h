// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_VIDEO_RATE_CONTROL_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_VIDEO_RATE_CONTROL_WRAPPER_H_

#include <array>
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
    RateControlConfig();
    ~RateControlConfig();

    RateControlConfig(const RateControlConfig&);
    RateControlConfig& operator=(const RateControlConfig&);

    // Frame size.
    int width = 0;
    int height = 0;
    // Quantizer parameter，the range is 0-63.
    int max_quantizer = 0;
    int min_quantizer = 0;
    // Target_bandwidth is in kbps.
    int64_t target_bandwidth = 0;
    // Frame rate.
    double framerate = 0.0f;
    // Content type, camera or display.
    VideoEncodeAccelerator::Config::ContentType content_type =
        VideoEncodeAccelerator::Config::ContentType::kCamera;
    // Target bitrate for svc layers.
    std::array<int, kMaxLayers> layer_target_bitrate = {};
    // Rate decimator for temporal layers.
    std::array<int, kMaxTemporalLayers> ts_rate_decimator = {};
    // Number of spatial layers.
    int ss_number_layers = 0;
    // Number of temporal layers.
    int ts_number_layers = 0;
    // Quantizer parameter for svc layers.
    std::array<int, kMaxLayers> max_quantizers = {};
    std::array<int, kMaxLayers> min_quantizers = {};
    // Scaling factor parameters for spatial layers.
    std::array<int, kMaxSpatialLayers> scaling_factor_num = {};
    std::array<int, kMaxSpatialLayers> scaling_factor_den = {};
    // If defined, the H.264 BRC uses fixed QP difference between layers. Should
    // not be defined for other SW BRCs.
    std::optional<int> fixed_delta_qp;
  };

  // FrameParams is used for passing frame params.
  struct FrameParams {
    enum class FrameType { kKeyFrame, kInterFrame };
    FrameType frame_type = FrameType::kKeyFrame;
    int spatial_layer_id = 0;
    int temporal_layer_id = 0;
    unsigned int timestamp = 0;
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
