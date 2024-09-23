// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_ENCODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_ENCODER_DELEGATE_H_

#include <memory>
#include <utility>
#include <vector>

#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/vaapi/vaapi_video_encoder_delegate.h"
#include "media/gpu/vp9_picture.h"
#include "media/gpu/vp9_reference_frame_vector.h"
#include "media/parsers/vp9_parser.h"
#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"

namespace media {
class VaapiWrapper;
class VP9SVCLayers;

// Wrapper for the libVPX VP9 rate controller that allows us to override methods
// for unit testing.
class VP9RateControlWrapper {
 public:
  static std::unique_ptr<VP9RateControlWrapper> Create(
      const libvpx::VP9RateControlRtcConfig& config);

  VP9RateControlWrapper();
  explicit VP9RateControlWrapper(
      std::unique_ptr<libvpx::VP9RateControlRTC> impl);
  virtual ~VP9RateControlWrapper();

  virtual void UpdateRateControl(
      const libvpx::VP9RateControlRtcConfig& rate_control_config);
  virtual libvpx::FrameDropDecision ComputeQP(
      const libvpx::VP9FrameParamsQpRTC& frame_params);
  virtual int GetQP() const;
  // GetLoopfilterLevel() needs to be called after ComputeQP().
  virtual int GetLoopfilterLevel() const;
  virtual void PostEncodeUpdate(
      uint64_t encoded_frame_size,
      const libvpx::VP9FrameParamsQpRTC& frame_params);

 private:
  const std::unique_ptr<libvpx::VP9RateControlRTC> impl_;
};

class VP9VaapiVideoEncoderDelegate : public VaapiVideoEncoderDelegate {
 public:
  struct EncodeParams {
    EncodeParams();

    // Produce a keyframe at least once per this many frames.
    size_t kf_period_frames;

    // Bitrate allocation in bps.
    VideoBitrateAllocation bitrate_allocation;

    // Framerate in FPS.
    uint32_t framerate;

    // Quantization parameter. They are vp9 ac/dc indices and their ranges are
    // 0-255.
    uint8_t min_qp;
    uint8_t max_qp;

    // The rate controller drop frame threshold. 0-100 as this is percentage.
    uint8_t drop_frame_thresh = 0;

    // The encoding content is a screen content.
    bool is_screen = false;

    bool error_resilident_mode = false;
  };

  VP9VaapiVideoEncoderDelegate(scoped_refptr<VaapiWrapper> vaapi_wrapper,
                               base::RepeatingClosure error_cb);

  VP9VaapiVideoEncoderDelegate(const VP9VaapiVideoEncoderDelegate&) = delete;
  VP9VaapiVideoEncoderDelegate& operator=(const VP9VaapiVideoEncoderDelegate&) =
      delete;

  ~VP9VaapiVideoEncoderDelegate() override;

  // VaapiVideoEncoderDelegate implementation.
  bool Initialize(const VideoEncodeAccelerator::Config& config,
                  const VaapiVideoEncoderDelegate::Config& ave_config) override;
  bool UpdateRates(const VideoBitrateAllocation& bitrate_allocation,
                   uint32_t framerate) override;
  gfx::Size GetCodedSize() const override;
  size_t GetMaxNumOfRefFrames() const override;
  std::vector<gfx::Size> GetSVCLayerResolutions() override;

 private:
  friend class VP9VaapiVideoEncoderDelegateTest;
  friend class VaapiVideoEncodeAcceleratorTest;

  void set_rate_ctrl_for_testing(
      std::unique_ptr<VP9RateControlWrapper> rate_ctrl);

  bool RecreateSVCLayersIfNeeded(VideoBitrateAllocation& bitrate_allocation);
  bool ApplyPendingUpdateRates();

  PrepareEncodeJobResult PrepareEncodeJob(EncodeJob& encode_job) override;
  BitstreamBufferMetadata GetMetadata(const EncodeJob& encode_job,
                                      size_t payload_size) override;
  void BitrateControlUpdate(const BitstreamBufferMetadata& metadata) override;

  Vp9FrameHeader GetDefaultFrameHeader(const bool keyframe) const;
  PrepareEncodeJobResult SetFrameHeader(
      bool keyframe,
      VP9Picture* picture,
      std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used);
  void UpdateReferenceFrames(scoped_refptr<VP9Picture> picture);

  bool SubmitFrameParameters(
      EncodeJob& job,
      const EncodeParams& encode_params,
      scoped_refptr<VP9Picture> pic,
      const Vp9ReferenceFrameVector& ref_frames,
      const std::array<bool, kVp9NumRefsPerFrame>& ref_frames_used);

  gfx::Size visible_size_;
  gfx::Size coded_size_;  // Macroblock-aligned.

  // Frame count since last keyframe, reset to 0 every keyframe period.
  size_t frame_num_ = 0;
  size_t ref_frame_index_ = 0;

  EncodeParams current_params_;

  Vp9ReferenceFrameVector reference_frames_;
  std::unique_ptr<VP9SVCLayers> svc_layers_;

  std::optional<std::pair<VideoBitrateAllocation, uint32_t>>
      pending_update_rates_;

  std::unique_ptr<VP9RateControlWrapper> rate_ctrl_;

  std::optional<base::TimeDelta> dropped_superframe_timestamp_;

  // TODO(b/297226972): Remove the workaround once the iHD driver is fixed.
  bool is_last_encoded_key_frame_ = false;
};
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_ENCODER_DELEGATE_H_
