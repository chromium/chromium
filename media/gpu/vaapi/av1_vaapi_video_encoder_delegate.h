// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_AV1_VAAPI_VIDEO_ENCODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_AV1_VAAPI_VIDEO_ENCODER_DELEGATE_H_

#include <stdint.h>

#include <vector>

#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/av1_builder.h"
#include "media/gpu/av1_picture.h"
#include "media/gpu/vaapi/vaapi_video_encoder_delegate.h"

namespace aom {
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

    // The rate controller drop frame threshold. 0-100 as this is percentage.
    uint8_t drop_frame_thresh = 0;

    // The encoding content is a screen content.
    bool is_screen = false;

    // Sensible default values for CDEF taken from
    // https://github.com/intel/libva-utils/blob/master/encode/av1encode.c
    // TODO: we may want to tune these parameters.
    uint8_t cdef_y_pri_strength[8] = {9, 12, 0, 6, 2, 4, 1, 2};
    uint8_t cdef_y_sec_strength[8] = {0, 2, 0, 0, 0, 1, 0, 1};
    uint8_t cdef_uv_pri_strength[8] = {9, 12, 0, 6, 2, 4, 1, 2};
    uint8_t cdef_uv_sec_strength[8] = {0, 2, 0, 0, 0, 1, 0, 1};
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
  BitstreamBufferMetadata GetMetadata(const EncodeJob& encode_job,
                                      size_t payload_size) override;
  PrepareEncodeJobResult PrepareEncodeJob(EncodeJob& encode_job) override;
  void BitrateControlUpdate(const BitstreamBufferMetadata& metadata) override;

  bool SubmitTemporalDelimiter(size_t& temporal_delimiter_obu_size);
  bool SubmitSequenceHeader(size_t& sequence_header_obu_size);
  bool SubmitSequenceParam();
  bool SubmitSequenceHeaderOBU(size_t& sequence_header_obu_size);
  bool SubmitFrame(const EncodeJob& job, size_t frame_header_obu_offset);
  bool FillPictureParam(VAEncPictureParameterBufferAV1& pic_param,
                        VAEncSegMapBufferAV1& segment_map_param,
                        const EncodeJob& job,
                        const AV1Picture& pic);
  bool SubmitFrameOBU(const VAEncPictureParameterBufferAV1& pic_param,
                      size_t& frame_header_obu_size_offset);
  bool SubmitPictureParam(const VAEncPictureParameterBufferAV1& pic_param);
  bool SubmitSegmentMap(const VAEncSegMapBufferAV1& segment_map_param);
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
  AV1BitstreamBuilder::SequenceHeader sequence_header_;
  std::unique_ptr<aom::AV1RateControlRTC> rate_ctrl_;
  std::vector<uint8_t> segmentation_map_{};
  uint32_t seg_size_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_AV1_VAAPI_VIDEO_ENCODER_DELEGATE_H_
