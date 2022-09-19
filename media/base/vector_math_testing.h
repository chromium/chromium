// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VECTOR_MATH_TESTING_H_
#define MEDIA_BASE_VECTOR_MATH_TESTING_H_

#include <utility>

#include "build/build_config.h"
#include "media/base/media_shmem_export.h"

namespace media {
namespace vector_math {

// Optimized versions exposed for testing.  See vector_math.h for details.
MEDIA_SHMEM_EXPORT void FMAC_C(const float src[],
                               float scale,
                               int len,
                               float dest[]);
MEDIA_SHMEM_EXPORT void FMUL_C(const float src[],
                               float scale,
                               int len,
                               float dest[]);
MEDIA_SHMEM_EXPORT std::pair<float, float> EWMAAndMaxPower_C(
    float initial_value,
    const float src[],
    int len,
    float smoothing_factor);

#if defined(ARCH_CPU_X86_FAMILY) && !BUILDFLAG(IS_NACL)
MEDIA_SHMEM_EXPORT void FMAC_SSE(const float src[],
                                 float scale,
                                 int len,
                                 float dest[]);
MEDIA_SHMEM_EXPORT void FMUL_SSE(const float src[],
                                 float scale,
                                 int len,
                                 float dest[]);
MEDIA_SHMEM_EXPORT std::pair<float, float> EWMAAndMaxPower_SSE(
    float initial_value,
    const float src[],
    int len,
    float smoothing_factor);
MEDIA_SHMEM_EXPORT void FMAC_AVX2(const float src[],
                                  float scale,
                                  int len,
                                  float dest[]);
MEDIA_SHMEM_EXPORT void FMUL_AVX2(const float src[],
                                  float scale,
                                  int len,
                                  float dest[]);
MEDIA_SHMEM_EXPORT std::pair<float, float> EWMAAndMaxPower_AVX2(
    float initial_value,
    const float src[],
    int len,
    float smoothing_factor);
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
MEDIA_SHMEM_EXPORT void FMAC_NEON(const float src[],
                                  float scale,
                                  int len,
                                  float dest[]);
MEDIA_SHMEM_EXPORT void FMUL_NEON(const float src[],
                                  float scale,
                                  int len,
                                  float dest[]);
MEDIA_SHMEM_EXPORT std::pair<float, float> EWMAAndMaxPower_NEON(
    float initial_value,
    const float src[],
    int len,
    float smoothing_factor);
#endif

}  // namespace vector_math
}  // namespace media

#endif  // MEDIA_BASE_VECTOR_MATH_TESTING_H_
