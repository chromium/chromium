// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_H264_RATECTRL_RTC_H_
#define MEDIA_GPU_H264_RATECTRL_RTC_H_

#include "media/gpu/h264_rate_controller.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

typedef H264RateControllerSettings H264RateControlConfigRTC;

// The parameters used by H264RateCtrlRTC class to compute the QP.
struct MEDIA_GPU_EXPORT H264FrameParamsRTC {
  bool keyframe;
  int temporal_layer_id;
  base::TimeDelta timestamp;
};

// The class is used as a parameter for instantiation of
// media::VideoRateControlWrapperInternal template class.
class MEDIA_GPU_EXPORT H264RateCtrlRTC {
 public:
  enum class FrameDropDecision {
    kOk,    // Frame is encoded.
    kDrop,  // Frame is dropped.
  };

  ~H264RateCtrlRTC();

  H264RateCtrlRTC(const H264RateCtrlRTC& other) = delete;
  H264RateCtrlRTC& operator=(const H264RateCtrlRTC& other) = delete;

  // Creates a new H264RateCtrlRTC instance with the given `config`.
  static std::unique_ptr<H264RateCtrlRTC> Create(
      const H264RateControlConfigRTC& config);

  // Updates Rate Control using the given `config`.
  void UpdateRateControl(const H264RateControlConfigRTC& config);

  // GetQP() needs to be called after ComputeQP() to get the latest QP.
  int GetQP();

  // Loop filter level is not used. The method returns 0.
  int GetLoopfilterLevel() const;

  // ComputeQP() returns kOk if the frame is to be encoded, and GetQP()
  // returns a valid QP value.
  // Otherwise it returns kDrop, GetQP() returns -1 and PostEncodeUpdate()
  // must not be invoked.
  FrameDropDecision ComputeQP(const H264FrameParamsRTC& frame_params);

  // Feedback to rate control with the size of current encoded frame.
  void PostEncodeUpdate(uint64_t encoded_frame_size,
                        const H264FrameParamsRTC& frame_params);

  // The `buffer_fullness` array is filled with the HRD buffer fullness values
  // for each layer at the given `timestamp`.
  void GetBufferFullness(base::span<int> buffer_fullness,
                         base::TimeDelta timestamp);

 private:
  explicit H264RateCtrlRTC(const H264RateControlConfigRTC& config);

  bool config_changed_ = false;
  H264RateControlConfigRTC config_;
  H264RateControlConfigRTC new_config_;
  int frame_qp_ = 0;

  H264RateController rate_controller_;
};
}  // namespace media

#endif  // MEDIA_GPU_H264_RATECTRL_RTC_H_
