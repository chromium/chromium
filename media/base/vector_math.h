// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VECTOR_MATH_H_
#define MEDIA_BASE_VECTOR_MATH_H_

#include <utility>

#include "media/base/media_shmem_export.h"

namespace media {
namespace vector_math {

// Required alignment for inputs and outputs to all vector math functions
enum { kRequiredAlignment = 16 };

// Multiply each element of |src| (up to |len|) by |scale| and add to |dest|.
// |src| and |dest| must be aligned by kRequiredAlignment.
MEDIA_SHMEM_EXPORT void FMAC(const float src[],
                             float scale,
                             int len,
                             float dest[]);

// Multiply each element of |src| by |scale| and store in |dest|.  |src| and
// |dest| must be aligned by kRequiredAlignment.
MEDIA_SHMEM_EXPORT void FMUL(const float src[],
                             float scale,
                             int len,
                             float dest[]);

// Computes the exponentially-weighted moving average power of a signal by
// iterating the recurrence:
//
//   y[-1] = initial_value
//   y[n] = smoothing_factor * src[n]^2 + (1-smoothing_factor) * y[n-1]
//
// Returns the final average power and the maximum squared element value.
MEDIA_SHMEM_EXPORT std::pair<float, float> EWMAAndMaxPower(
    float initial_value,
    const float src[],
    int len,
    float smoothing_factor);

}  // namespace vector_math
}  // namespace media

#endif  // MEDIA_BASE_VECTOR_MATH_H_
