// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VP8_VAAPI_VIDEO_ENCODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_VP8_VAAPI_VIDEO_ENCODER_DELEGATE_H_

#include <vector>

#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/vaapi/vaapi_video_encoder_delegate.h"
#include "media/gpu/video_rate_control.h"
#include "media/gpu/vp8_picture.h"
#include "media/gpu/vp8_reference_frame_vector.h"
#include "media/parsers/vp8_parser.h"

namespace libvpx {
struct VP8FrameParamsQpRTC;
class VP8RateControlRTC;
struct VP8RateControlRtcConfig;
}  // namespace libvpx

namespace media {
class VaapiWrapper;

class VP8VaapiVideoEncoderDelegate : public VaapiVideoEncoderDelegate {
 public:
  struct EncodeParams {
    EncodeParams();

    // Produce a keyframe at least once per this many frames.
    size_t kf_period_frames;

    // Bitrate allocation in bps.
    VideoBitrateAllocation bitrate_allocation;

    // Framerate in FPS.
    uint32_t framerate;

    // Quantization parameter. They are vp8 ac/dc indices and their ranges are
    // 0-127.
    uint8_t min_qp;
    uint8_t max_qp;
  };

  VP8VaapiVideoEncoderDelegate(scoped_refptr<VaapiWrapper> vaapi_wrapper,
                               base::RepeatingClosure error_cb);

  VP8VaapiVideoEncoderDelegate(const VP8VaapiVideoEncoderDelegate&) = delete;
  VP8VaapiVideoEncoderDelegate& operator=(const VP8VaapiVideoEncoderDelegate&) =
      delete;

  ~VP8VaapiVideoEncoderDelegate() override;

  // VaapiVideoEncoderDelegate implementation.
  bool Initialize(const VideoEncodeAccelerator::Config& config,
                  const VaapiVideoEncoderDelegate::Config& ave_config) override;
  bool UpdateRates(const VideoBitrateAllocation& bitrate_allocation,
                   uint32_t framerate) override;
  gfx::Size GetCodedSize() const override;
  size_t GetMaxNumOfRefFrames() const override;
  std::vector<gfx::Size> GetSVCLayerResolutions() override;

 private:
  void InitializeFrameHeader();

  void SetFrameHeader(
      size_t frame_num,
      VP8Picture& picture,
      std::array<bool, kNumVp8ReferenceBuffers>& ref_frames_used);
  void UpdateReferenceFrames(scoped_refptr<VP8Picture> picture);
  void Reset();

  bool PrepareEncodeJob(EncodeJob& encode_job) override;
  BitstreamBufferMetadata GetMetadata(const EncodeJob& encode_job,
                                      size_t payload_size) override;
  void BitrateControlUpdate(uint64_t encoded_chunk_size_bytes) override;

  bool SubmitFrameParameters(
      EncodeJob& job,
      const EncodeParams& encode_params,
      scoped_refptr<VP8Picture> pic,
      const Vp8ReferenceFrameVector& ref_frames,
      const std::array<bool, kNumVp8ReferenceBuffers>& ref_frames_used);

  gfx::Size visible_size_;
  gfx::Size coded_size_;  // Macroblock-aligned.

  uint8_t num_temporal_layers_ = 1;

  // Frame count since last keyframe, reset to 0 every keyframe period.
  size_t frame_num_ = 0;

  EncodeParams current_params_;

  Vp8ReferenceFrameVector reference_frames_;

  using VP8RateControl = VideoRateControl<libvpx::VP8RateControlRtcConfig,
                                          libvpx::VP8RateControlRTC,
                                          libvpx::VP8FrameParamsQpRTC>;
  std::unique_ptr<VP8RateControl> rate_ctrl_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VP8_VAAPI_VIDEO_ENCODER_DELEGATE_H_
