// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VECTOR_MATH_TESTING_H_
#define MEDIA_BASE_VECTOR_MATH_TESTING_H_

#include <utility>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

namespace media {
namespace vector_math {

// Optimized versions exposed for testing.  See vector_math.h for details.
MEDIA_EXPORT void FMAC_C(base::span<const float> src,
                         float scale,
                         base::span<float> dest);
MEDIA_EXPORT void FMUL_C(base::span<const float> src,
                         float scale,
                         base::span<float> dest);
MEDIA_EXPORT void FCLAMP_C(base::span<const float> src, base::span<float> dest);
MEDIA_EXPORT std::pair<float, float> EWMAAndMaxPower_C(
    float initial_value,
    base::span<const float> src,
    float smoothing_factor);

#if defined(ARCH_CPU_X86_FAMILY)
MEDIA_EXPORT void FMAC_SSE(base::span<const float> src,
                           float scale,
                           base::span<float> dest);
MEDIA_EXPORT void FMUL_SSE(base::span<const float> src,
                           float scale,
                           base::span<float> dest);
MEDIA_EXPORT void FCLAMP_SSE(base::span<const float> src,
                             base::span<float> dest);
MEDIA_EXPORT std::pair<float, float> EWMAAndMaxPower_SSE(
    float initial_value,
    base::span<const float> src,
    float smoothing_factor);
MEDIA_EXPORT void FMAC_AVX2(base::span<const float> src,
                            float scale,
                            base::span<float> dest);
MEDIA_EXPORT void FMUL_AVX2(base::span<const float> src,
                            float scale,
                            base::span<float> dest);
MEDIA_EXPORT void FCLAMP_AVX(base::span<const float> src,
                             base::span<float> dest);
MEDIA_EXPORT std::pair<float, float> EWMAAndMaxPower_AVX2(
    float initial_value,
    base::span<const float> src,
    float smoothing_factor);
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
MEDIA_EXPORT void FMAC_NEON(base::span<const float> src,
                            float scale,
                            base::span<float> dest);
MEDIA_EXPORT void FMUL_NEON(base::span<const float> src,
                            float scale,
                            base::span<float> dest);
MEDIA_EXPORT void FCLAMP_NEON(base::span<const float> src,
                              base::span<float> dest);
MEDIA_EXPORT std::pair<float, float> EWMAAndMaxPower_NEON(
    float initial_value,
    base::span<const float> src,
    float smoothing_factor);
#endif

}  // namespace vector_math
}  // namespace media

#endif  // MEDIA_BASE_VECTOR_MATH_TESTING_H_
