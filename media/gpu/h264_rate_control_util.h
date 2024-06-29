// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_H264_RATE_CONTROL_UTIL_H_
#define MEDIA_GPU_H264_RATE_CONTROL_UTIL_H_

#include "base/time/time.h"
#include "media/gpu/media_gpu_export.h"

// Utility methods used by H.264 Rate Controller and dependent classes.
namespace media::h264_rate_control_util {
// The maximum number of temporal layers in the stream.
inline constexpr size_t kMaxNumTemporalLayers = 2u;

// The upper and the lower bounds of QP value.
inline constexpr uint32_t kQPMax = 51u;
inline constexpr uint32_t kQPMin = 1u;

// The scaling procedure for the 4x4 transform coefficients specified in the
// H.264 standard, sections 8.5.9 and 8.5.12.1, is approximated by the relation
// between QP and Q step size. The quantizer step size for the transform
// coefficient at position (0,0) is 5/8 when the QP is 0. This value is used as
// a constant multiplier in the expression that converts QP to Q step size.
// q_step = 5/8 * 2^(qp/6)
MEDIA_GPU_EXPORT float QP2QStepSize(uint32_t qp);
MEDIA_GPU_EXPORT uint32_t QStepSize2QP(float q_step);

// Returns the difference between two timestamps. The difference is limited to
// the interval 0 - 5 minutes. The interval is chosen arbitrarily, long enough
// to support specific use cases.
MEDIA_GPU_EXPORT base::TimeDelta ClampedTimestampDiff(base::TimeDelta ts_new,
                                                      base::TimeDelta ts_old);

// The x value is clamped to the interval [x0, x1]. The y value is linearly
// interpolated between the points (x0, y0) and (x1, y1).
MEDIA_GPU_EXPORT float ClampedLinearInterpolation(float x,
                                                  float x0,
                                                  float x1,
                                                  float y0,
                                                  float y1);
}  // namespace media::h264_rate_control_util

#endif  // MEDIA_GPU_H264_RATE_CONTROL_UTIL_H_
