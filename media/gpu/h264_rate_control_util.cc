// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h264_rate_control_util.h"

#include <algorithm>

#include "base/logging.h"

namespace media::h264_rate_control_util {

float QP2QStepSize(uint32_t qp) {
  // 0.625 is Q-step value for QP=0 for H.26x codecs.
  return 0.625f * std::pow(2, qp / 6.0f);
}

uint32_t QStepSize2QP(float q_step) {
  const float q_step_min = QP2QStepSize(kQPMin);
  const float q_step_max = QP2QStepSize(kQPMax);
  q_step = std::clamp(q_step, q_step_min, q_step_max);
  // 0.625 is Q-step value for QP=0 for H.26x codecs.
  return static_cast<uint32_t>(std::round(6 * std::log2(q_step / 0.625f)));
}

base::TimeDelta ClampedTimestampDiff(base::TimeDelta ts_new,
                                     base::TimeDelta ts_old) {
  base::TimeDelta elapsed_time = ts_new - ts_old;
  base::TimeDelta clamped_elapsed_time =
      std::clamp(elapsed_time, base::Seconds(0), base::Minutes(5));
  if (clamped_elapsed_time != elapsed_time) {
    DLOG(WARNING) << "Unexpected elapsed time " << elapsed_time;
  }
  return clamped_elapsed_time;
}

float ClampedLinearInterpolation(float x,
                                 float x0,
                                 float x1,
                                 float y0,
                                 float y1) {
  return std::clamp(y0 + (x - x0) * (y1 - y0) / (x1 - x0), std::min(y0, y1),
                    std::max(y0, y1));
}

}  // namespace media::h264_rate_control_util
