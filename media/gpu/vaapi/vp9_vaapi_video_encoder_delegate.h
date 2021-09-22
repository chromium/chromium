// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_ENCODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_ENCODER_DELEGATE_H_

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/vaapi/vaapi_video_encoder_delegate.h"
#include "media/gpu/vp9_picture.h"
#include "media/gpu/vp9_reference_frame_vector.h"

namespace media {
class VaapiWrapper;
class VP9SVCLayers;
class VP9RateControl;

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
    uint8_t initial_qp;
    uint8_t min_qp;
    uint8_t max_qp;

    bool error_resilient_mode;
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
  bool PrepareEncodeJob(EncodeJob* encode_job) override;
  void BitrateControlUpdate(uint64_t encoded_chunk_size_bytes) override;
  BitstreamBufferMetadata GetMetadata(EncodeJob* encode_job,
                                      size_t payload_size) override;
  std::vector<gfx::Size> GetSVCLayerResolutions() override;

 private:
  friend class VP9VaapiVideoEncoderDelegateTest;
  friend class VaapiVideoEncodeAcceleratorTest;

  void set_rate_ctrl_for_testing(std::unique_ptr<VP9RateControl> rate_ctrl);

  bool ApplyPendingUpdateRates();

  Vp9FrameHeader GetDefaultFrameHeader(const bool keyframe) const;
  void SetFrameHeader(bool keyframe,
                      VP9Picture* picture,
                      std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used);
  void UpdateReferenceFrames(scoped_refptr<VP9Picture> picture);

  // Gets the encoded chunk size whose id is |buffer_id| and updates the bitrate
  // control.
  void NotifyEncodedChunkSize(VABufferID buffer_id,
                              VASurfaceID sync_surface_id);

  scoped_refptr<VP9Picture> GetPicture(EncodeJob* job);

  bool SubmitFrameParameters(
      EncodeJob* job,
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

  absl::optional<std::pair<VideoBitrateAllocation, uint32_t>>
      pending_update_rates_;

  std::unique_ptr<VP9RateControl> rate_ctrl_;
};
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_ENCODER_DELEGATE_H_
