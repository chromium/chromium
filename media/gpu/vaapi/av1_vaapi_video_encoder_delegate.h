// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_AV1_VAAPI_VIDEO_ENCODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_AV1_VAAPI_VIDEO_ENCODER_DELEGATE_H_

#include <vector>

#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/av1_picture.h"
#include "media/gpu/vaapi/vaapi_video_encoder_delegate.h"
#include "media/gpu/video_rate_control.h"

namespace aom {
struct AV1RateControlRtcConfig;
struct AV1FrameParamsRTC;
class AV1RateControlRTC;
}  // namespace aom

namespace media {

class AV1VaapiVideoEncoderDelegate : public VaapiVideoEncoderDelegate {
 public:
  struct EncodeParams {
    EncodeParams();

    size_t intra_period = 0;  // Period between keyframes
    VideoBitrateAllocation bitrate_allocation;
    uint32_t framerate = 0;
    uint8_t min_qp = 0;
    uint8_t max_qp = 0;
    // Sensible default values for CDEF taken from
    // https://github.com/intel/libva-utils/blob/master/encode/av1encode.c
    // TODO: we may want to tune these parameters.
    uint8_t cdef_y_pri_strength[8] = {9, 12, 0, 6, 2, 4, 1, 2};
    uint8_t cdef_y_sec_strength[8] = {0, 2, 0, 0, 0, 1, 0, 1};
    uint8_t cdef_uv_pri_strength[8] = {9, 12, 0, 6, 2, 4, 1, 2};
    uint8_t cdef_uv_sec_strength[8] = {0, 2, 0, 0, 0, 1, 0, 1};
  };

  struct PicParamOffsets {
    uint32_t q_idx_bit_offset = 0;
    uint32_t segmentation_bit_offset = 0;
    uint32_t segmentation_bit_size = 0;
    uint32_t loop_filter_params_bit_offset = 0;
    uint32_t frame_hdr_obu_size_bits = 0;
    uint32_t frame_hdr_obu_size_byte_offset = 0;  // Tell the driver where to
                                                  // put the frame size
    uint32_t uncompressed_hdr_byte_offset = 0;
    uint32_t cdef_params_bit_offset = 0;
    uint32_t cdef_params_size_bits = 0;
  };

  AV1VaapiVideoEncoderDelegate(scoped_refptr<VaapiWrapper> vaapi_wrapper,
                               base::RepeatingClosure error_cb);

  AV1VaapiVideoEncoderDelegate(const AV1VaapiVideoEncoderDelegate&) = delete;
  AV1VaapiVideoEncoderDelegate& operator=(const AV1VaapiVideoEncoderDelegate&) =
      delete;

  ~AV1VaapiVideoEncoderDelegate() override;

  // VaapiVideoEncoderDelegate implementation
  bool Initialize(const VideoEncodeAccelerator::Config& config,
                  const VaapiVideoEncoderDelegate::Config& ave_config) override;
  bool UpdateRates(const VideoBitrateAllocation& bitrate_allocation,
                   uint32_t framerate) override;
  gfx::Size GetCodedSize() const override;
  size_t GetMaxNumOfRefFrames() const override;
  std::vector<gfx::Size> GetSVCLayerResolutions() override;

 private:
  using AV1RateControl = VideoRateControl<aom::AV1RateControlRtcConfig,
                                          aom::AV1RateControlRTC,
                                          aom::AV1FrameParamsRTC>;

  BitstreamBufferMetadata GetMetadata(const EncodeJob& encode_job,
                                      size_t payload_size) override;
  bool PrepareEncodeJob(EncodeJob& encode_job) override;
  void BitrateControlUpdate(const BitstreamBufferMetadata& metadata) override;

  bool SubmitTemporalDelimiter(PicParamOffsets& offsets);
  bool SubmitSequenceHeader(PicParamOffsets& offsets);
  bool SubmitSequenceParam();
  bool SubmitSequenceHeaderOBU(PicParamOffsets& offsets);
  std::vector<uint8_t> PackSequenceHeader() const;
  bool SubmitFrame(EncodeJob& job, PicParamOffsets& offsets);
  bool FillPictureParam(VAEncPictureParameterBufferAV1& pic_param,
                        const EncodeJob& job,
                        const AV1Picture& pic) const;
  bool SubmitFrameOBU(const VAEncPictureParameterBufferAV1& pic_param,
                      PicParamOffsets& offsets);
  std::vector<uint8_t> PackFrameHeader(
      const VAEncPictureParameterBufferAV1& pic_param,
      PicParamOffsets& offsets) const;
  bool SubmitPictureParam(VAEncPictureParameterBufferAV1& pic_param,
                          const PicParamOffsets& offsets);
  bool SubmitTileGroup();
  bool SubmitPackedData(const std::vector<uint8_t>& data);

  int level_idx_;
  uint64_t frame_num_ = 0;
  EncodeParams current_params_;
  gfx::Size visible_size_;
  gfx::Size coded_size_;
  // TODO(b:274756117): In tuning this encoder, we may decide we want multiple
  // reference frames, not just the most recent.
  scoped_refptr<AV1Picture> last_frame_ = nullptr;
  VAEncSequenceParameterBufferAV1 seq_param_;
  std::unique_ptr<AV1RateControl> rate_ctrl_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_AV1_VAAPI_VIDEO_ENCODER_DELEGATE_H_
