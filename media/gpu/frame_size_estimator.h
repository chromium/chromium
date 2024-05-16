// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_FRAME_SIZE_ESTIMATOR_H_
#define MEDIA_GPU_FRAME_SIZE_ESTIMATOR_H_

#include "base/time/time.h"
#include "media/gpu/exponential_moving_average.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// An encoded frame size estimator.
// The estimator maintains the history of intermediate values (qp_size_value)
// that are proportional to encoded frame size and QP, and inversely
// proportional to the QP ratio of the previous and the current frame
// (delta_q_step_factor). The QP is converted to Q step value that has linear
// dependency to the encoded frame size.
//
// q_step = 5 / 8 * 2^(qp / 6)
//
// delta_q_step_factor = q_step_prev / q_step
//
// qp_size_value = q_step * frame_bytes / delta_q_step_factor
//
// The prediction of the encoded frame size is based on average values of
// qp_size_value and qp_size_correction. The qp_size_correction is the
// difference between actual encoded bytes and the predicted value.
//
// qp_size_correction = frame_bytes -
//                      qp_size_value * delta_q_step_factor / q_step
//
// pred_frame_bytes =
//     qp_size_value * delta_q_step_factor / q_step + qp_size_correction
//
// In the inverse estimation process, the QP value is estimated for the given
// encoded frame size and the QP of the previous frame.
//
// q_step =
//     sqrt(qp_size_value * q_step_prev / (frame_bytes + qp_size_correction))
//
// qp = 6 * log2(q_step / (5 / 8))
class MEDIA_GPU_EXPORT FrameSizeEstimator {
 public:
  FrameSizeEstimator(base::TimeDelta max_window_size,
                     float initial_qp_size,
                     float initial_size_correction);
  ~FrameSizeEstimator();

  FrameSizeEstimator(const FrameSizeEstimator& other) = delete;
  FrameSizeEstimator& operator=(const FrameSizeEstimator& other) = delete;

  float qp_size_mean() const { return qp_size_stats_.mean(); }
  float size_correction_mean() const { return size_correction_stats_.mean(); }

  // Estimates encoded frame size for the given `qp` and `qp_prev`, based on the
  // stats of the previous frames. In usual encoding scenario, the current
  // QP is unknown at this point, but the estimate of the QP parameter is used
  // instead.
  size_t Estimate(uint32_t qp, uint32_t qp_prev) const;

  // The Inverse Estimate is a reverse operation in frame size estimation where
  // the QP value of the current frame is evaluated from the stats of the
  // previously encoded frames and the encoded frame size.
  uint32_t InverseEstimate(size_t target_frame_bytes, uint32_t qp_prev) const;

  // Updates the frame size estimator state with the real encoded frame size and
  // with the parameters used for video frame encoding.
  void Update(size_t frame_bytes,
              uint32_t qp,
              uint32_t qp_prev,
              base::TimeDelta elapsed_time);

  void UpdateMaxWindowSize(base::TimeDelta max_window_size);

 private:
  ExponentialMovingAverage qp_size_stats_;
  ExponentialMovingAverage size_correction_stats_;
};

}  // namespace media

#endif  // MEDIA_GPU_FRAME_SIZE_ESTIMATOR_H_
