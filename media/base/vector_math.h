// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VECTOR_MATH_H_
#define MEDIA_BASE_VECTOR_MATH_H_

#include <utility>

#include "base/containers/span.h"
#include "media/base/media_shmem_export.h"

namespace media::vector_math {

// Required alignment for inputs and outputs to all vector math functions
enum { kRequiredAlignment = 16 };

// Multiply each element of `src` by `scale` and add to `dest`.
// `src` and `dest` must be aligned by `kRequiredAlignment`.
MEDIA_SHMEM_EXPORT void FMAC(base::span<const float> src,
                             float scale,
                             base::span<float> dest);

// Multiply each element of `src` by `scale` and store in `dest`.
// `src` and `dest` must be aligned by `kRequiredAlignment`.
MEDIA_SHMEM_EXPORT void FMUL(base::span<const float> src,
                             float scale,
                             base::span<float> dest);

// Clamps each element in `src` to the [-1.0, +1.0] range and store in `dest`.
// replacing NaNs with 0s (silence).
// `src` and `dest` must be aligned by `kRequiredAlignment`.
MEDIA_SHMEM_EXPORT void FCLAMP(base::span<const float> src,
                               base::span<float> dest);

// Computes the exponentially-weighted moving average power of a signal by
// iterating the recurrence:
//
//   y[-1] = initial_value
//   y[n] = smoothing_factor * src[n]^2 + (1-smoothing_factor) * y[n-1]
//
// Returns the final average power and the maximum squared element value.
MEDIA_SHMEM_EXPORT std::pair<float, float> EWMAAndMaxPower(
    float initial_value,
    base::span<const float> src,
    float smoothing_factor);

}  // namespace media::vector_math

#endif  // MEDIA_BASE_VECTOR_MATH_H_
