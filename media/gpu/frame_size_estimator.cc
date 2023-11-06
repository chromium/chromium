// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/frame_size_estimator.h"

#include <cmath>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"

namespace media {
namespace {

// Maps QP to quantizer step size. 0.625 is Q-step value for QP=0 for H.26x
// codecs.
float Qp2QStepSize(uint32_t qp) {
  return 0.625f * std::pow(2.0f, static_cast<float>(qp) / 6.0f);
}

void CalculateQSteps(uint32_t qp,
                     uint32_t qp_prev,
                     float& q_step,
                     float& q_step_prev,
                     float& delta_q_step_factor) {
  q_step = Qp2QStepSize(qp);
  q_step_prev = Qp2QStepSize(qp_prev);
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
