// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/frame_size_estimator.h"

#include <cmath>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "media/gpu/h264_rate_control_util.h"

namespace media {
namespace {
void CalculateQSteps(uint32_t qp,
                     uint32_t qp_prev,
                     float& q_step,
                     float& q_step_prev,
                     float& delta_q_step_factor) {
  q_step = h264_rate_control_util::QP2QStepSize(qp);
  q_step_prev = h264_rate_control_util::QP2QStepSize(qp_prev);
  delta_q_step_factor = q_step_prev / q_step;
}

}  // namespace

FrameSizeEstimator::FrameSizeEstimator(base::TimeDelta max_window_size,
                                       float initial_qp_size,
                                       float initial_size_correction)
    : qp_size_stats_(max_window_size), size_correction_stats_(max_window_size) {
  // The elapsed time is initially set to 1 millisecond to match the minimum
  // window size.
  qp_size_stats_.AddValue(initial_qp_size, base::Milliseconds(1));
  size_correction_stats_.AddValue(initial_size_correction,
                                  base::Milliseconds(1));
}

FrameSizeEstimator::~FrameSizeEstimator() = default;

size_t FrameSizeEstimator::Estimate(uint32_t qp, uint32_t qp_prev) const {
  float q_step, q_step_prev, delta_q_step_factor;
  CalculateQSteps(qp, qp_prev, q_step, q_step_prev, delta_q_step_factor);
  float pred_frame_byte = qp_size_stats_.mean() * delta_q_step_factor / q_step +
                          size_correction_stats_.mean();
  return static_cast<size_t>(std::max(pred_frame_byte, 0.0f));
}

uint32_t FrameSizeEstimator::InverseEstimate(size_t target_frame_bytes,
                                             uint32_t qp_prev) const {
  // The minimum target frame size is set to 20% of the target frame size.
  constexpr float kMinTargetBytesMultiplier = 0.2f;

  float frame_bytes = static_cast<float>(target_frame_bytes);

  frame_bytes -= size_correction_stats_.mean();
  frame_bytes =
      std::max(frame_bytes, target_frame_bytes * kMinTargetBytesMultiplier);

  const float qp_size = qp_size_stats_.mean();
  const float q_step_prev = h264_rate_control_util::QP2QStepSize(qp_prev);
  float q_step_base_square = q_step_prev * qp_size / frame_bytes;
  return h264_rate_control_util::QStepSize2QP(sqrtf(q_step_base_square));
}

void FrameSizeEstimator::Update(size_t frame_bytes,
                                uint32_t qp,
                                uint32_t qp_prev,
                                base::TimeDelta elapsed_time) {
  float q_step, q_step_prev, delta_q_step_factor;
  CalculateQSteps(qp, qp_prev, q_step, q_step_prev, delta_q_step_factor);

  float qp_size = q_step * frame_bytes / delta_q_step_factor;
  qp_size_stats_.AddValue(qp_size, elapsed_time);

  float corr =
      frame_bytes - qp_size_stats_.mean() * delta_q_step_factor / q_step;
  size_correction_stats_.AddValue(corr, elapsed_time);
}

void FrameSizeEstimator::UpdateMaxWindowSize(base::TimeDelta max_window_size) {
  qp_size_stats_.update_max_window_size(max_window_size);
  size_correction_stats_.update_max_window_size(max_window_size);
}

}  // namespace media
