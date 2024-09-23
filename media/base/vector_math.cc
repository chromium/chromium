// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/vector_math.h"
#include "media/base/vector_math_testing.h"

#include <algorithm>
#include <cmath>

#include "base/check_op.h"
#include "base/cpu.h"
#include "base/memory/aligned_memory.h"
#include "build/build_config.h"

// NaCl does not allow intrinsics.
#if defined(ARCH_CPU_X86_FAMILY) && !BUILDFLAG(IS_NACL)
#include <immintrin.h>
// Including these headers directly should generally be avoided. Since
// Chrome is compiled with -msse3 (the minimal requirement), we include the
// headers directly to make the intrinsics available.
#include <avxintrin.h>
#include <avx2intrin.h>
#include <fmaintrin.h>
// TODO(pcc): Linux currently uses ThinLTO which has broken auto-vectorization
// in clang, so use our intrinsic version for now. http://crbug.com/738085
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#include <arm_neon.h>
#endif

namespace media {
namespace vector_math {

void FMAC(const float src[], float scale, int len, float dest[]) {
  DCHECK(base::IsAligned(src, kRequiredAlignment));
  DCHECK(base::IsAligned(dest, kRequiredAlignment));
  static const auto fmac_func = [] {
#if defined(ARCH_CPU_X86_FAMILY) && !BUILDFLAG(IS_NACL)
    base::CPU cpu;
    if (cpu.has_avx2() && cpu.has_fma3())
      return FMAC_AVX2;
    return FMAC_SSE;
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
    return FMAC_NEON;
#else
    return FMAC_C;
#endif
  }();

  return fmac_func(src, scale, len, dest);
}

void FMAC_C(const float src[], float scale, int len, float dest[]) {
  for (int i = 0; i < len; ++i)
    dest[i] += src[i] * scale;
}

void FMUL(const float src[], float scale, int len, float dest[]) {
  DCHECK(base::IsAligned(src, kRequiredAlignment));
  DCHECK(base::IsAligned(dest, kRequiredAlignment));
  static const auto fmul_func = [] {
#if defined(ARCH_CPU_X86_FAMILY) && !BUILDFLAG(IS_NACL)
    base::CPU cpu;
    if (cpu.has_avx2())
      return FMUL_AVX2;
    return FMUL_SSE;
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
    return FMUL_NEON;
#else
    return FMUL_C;
#endif
  }();

  return fmul_func(src, scale, len, dest);
}

void FMUL_C(const float src[], float scale, int len, float dest[]) {
  for (int i = 0; i < len; ++i)
    dest[i] = src[i] * scale;
}

std::pair<float, float> EWMAAndMaxPower(
    float initial_value, const float src[], int len, float smoothing_factor) {
  DCHECK(base::IsAligned(src, kRequiredAlignment));
  static const auto ewma_and_max_power_func = [] {
#if defined(ARCH_CPU_X86_FAMILY) && !BUILDFLAG(IS_NACL)
    base::CPU cpu;
    if (cpu.has_avx2() && cpu.has_fma3())
      return EWMAAndMaxPower_AVX2;
    return EWMAAndMaxPower_SSE;
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
    return EWMAAndMaxPower_NEON;
#else
    return EWMAAndMaxPower_C;
#endif
  }();

  return ewma_and_max_power_func(initial_value, src, len, smoothing_factor);
}

std::pair<float, float> EWMAAndMaxPower_C(
    float initial_value, const float src[], int len, float smoothing_factor) {
  std::pair<float, float> result(initial_value, 0.0f);
  const float weight_prev = 1.0f - smoothing_factor;
  for (int i = 0; i < len; ++i) {
    result.first *= weight_prev;
    const float sample = src[i];
    const float sample_squared = sample * sample;
    result.first += sample_squared * smoothing_factor;
    result.second = std::max(result.second, sample_squared);
  }
  return result;
}

#if defined(ARCH_CPU_X86_FAMILY) && !BUILDFLAG(IS_NACL)
void FMUL_SSE(const float src[], float scale, int len, float dest[]) {
  const int rem = len % 4;
  const int last_index = len - rem;
  __m128 m_scale = _mm_set_ps1(scale);
  for (int i = 0; i < last_index; i += 4)
    _mm_store_ps(dest + i, _mm_mul_ps(_mm_load_ps(src + i), m_scale));

  // Handle any remaining values that wouldn't fit in an SSE pass.
  for (int i = last_index; i < len; ++i)
    dest[i] = src[i] * scale;
}

__attribute__((target("avx2"))) void FMUL_AVX2(const float src[],
                                               float scale,
                                               int len,
                                               float dest[]) {
  const int rem = len % 8;
  const int last_index = len - rem;
  __m256 m_scale = _mm256_set1_ps(scale);
  // TODO(crbug.com/40756517): Remove below alignment conditionals when AudioBus
  // |kChannelAlignment| updated to 32.
  bool aligned_src = (reinterpret_cast<uintptr_t>(src) & 0x1F) == 0;
  bool aligned_dest = (reinterpret_cast<uintptr_t>(dest) & 0x1F) == 0;
  if (aligned_src) {
    if (aligned_dest) {
      for (int i = 0; i < last_index; i += 8)
        _mm256_store_ps(dest + i,
                        _mm256_mul_ps(_mm256_load_ps(src + i), m_scale));
    } else {
      for (int i = 0; i < last_index; i += 8)
        _mm256_storeu_ps(dest + i,
                         _mm256_mul_ps(_mm256_load_ps(src + i), m_scale));
    }
  } else {
    if (aligned_dest) {
      for (int i = 0; i < last_index; i += 8)
        _mm256_store_ps(dest + i,
                        _mm256_mul_ps(_mm256_loadu_ps(src + i), m_scale));
    } else {
      for (int i = 0; i < last_index; i += 8)
        _mm256_storeu_ps(dest + i,
                         _mm256_mul_ps(_mm256_loadu_ps(src + i), m_scale));
    }
  }

  // Handle any remaining values that wouldn't fit in an SSE pass.
  for (int i = last_index; i < len; ++i)
    dest[i] = src[i] * scale;
}

void FMAC_SSE(const float src[], float scale, int len, float dest[]) {
  const int rem = len % 4;
  const int last_index = len - rem;
  __m128 m_scale = _mm_set_ps1(scale);
  for (int i = 0; i < last_index; i += 4) {
    _mm_store_ps(dest + i, _mm_add_ps(_mm_load_ps(dest + i),
                 _mm_mul_ps(_mm_load_ps(src + i), m_scale)));
  }

  // Handle any remaining values that wouldn't fit in an SSE pass.
  for (int i = last_index; i < len; ++i)
    dest[i] += src[i] * scale;
}

__attribute__((target("avx2,fma"))) void FMAC_AVX2(const float src[],
                                                   float scale,
                                                   int len,
                                                   float dest[]) {
  const int rem = len % 8;
  const int last_index = len - rem;
  __m256 m_scale = _mm256_set1_ps(scale);
  // TODO(crbug.com/40756517): Remove below alignment conditionals when AudioBus
  // |kChannelAlignment| updated to 32.
  bool aligned_src = (reinterpret_cast<uintptr_t>(src) & 0x1F) == 0;
  bool aligned_dest = (reinterpret_cast<uintptr_t>(dest) & 0x1F) == 0;
  if (aligned_src) {
    if (aligned_dest) {
      for (int i = 0; i < last_index; i += 8)
        _mm256_store_ps(dest + i,
                        _mm256_fmadd_ps(_mm256_load_ps(src + i), m_scale,
                                        _mm256_load_ps(dest + i)));
    } else {
      for (int i = 0; i < last_index; i += 8)
        _mm256_storeu_ps(dest + i,
                         _mm256_fmadd_ps(_mm256_load_ps(src + i), m_scale,
                                         _mm256_loadu_ps(dest + i)));
    }
  } else {
    if (aligned_dest) {
      for (int i = 0; i < last_index; i += 8)
        _mm256_store_ps(dest + i,
                        _mm256_fmadd_ps(_mm256_loadu_ps(src + i), m_scale,
                                        _mm256_load_ps(dest + i)));
    } else {
      for (int i = 0; i < last_index; i += 8)
        _mm256_storeu_ps(dest + i,
                         _mm256_fmadd_ps(_mm256_loadu_ps(src + i), m_scale,
                                         _mm256_loadu_ps(dest + i)));
    }
  }

  // Handle any remaining values that wouldn't fit in an SSE pass.
  for (int i = last_index; i < len; ++i)
    dest[i] += src[i] * scale;
}

// Convenience macro to extract float 0 through 3 from the vector |a|.  This is
// needed because compilers other than clang don't support access via
// operator[]().
#define EXTRACT_FLOAT(a, i) \
    (i == 0 ? \
         _mm_cvtss_f32(a) : \
         _mm_cvtss_f32(_mm_shuffle_ps(a, a, i)))

std::pair<float, float> EWMAAndMaxPower_SSE(
    float initial_value, const float src[], int len, float smoothing_factor) {
  // When the recurrence is unrolled, we see that we can split it into 4
  // separate lanes of evaluation:
  //
  // y[n] = a(S[n]^2) + (1-a)(y[n-1])
  //      = a(S[n]^2) + (1-a)^1(aS[n-1]^2) + (1-a)^2(aS[n-2]^2) + ...
  //      = z[n] + (1-a)^1(z[n-1]) + (1-a)^2(z[n-2]) + (1-a)^3(z[n-3])
  //
  // where z[n] = a(S[n]^2) + (1-a)^4(z[n-4]) + (1-a)^8(z[n-8]) + ...
  //
  // Thus, the strategy here is to compute z[n], z[n-1], z[n-2], and z[n-3] in
  // each of the 4 lanes, and then combine them to give y[n].

  const int rem = len % 4;
  const int last_index = len - rem;

  const __m128 smoothing_factor_x4 = _mm_set_ps1(smoothing_factor);
  const float weight_prev = 1.0f - smoothing_factor;
  const __m128 weight_prev_x4 = _mm_set_ps1(weight_prev);
  const __m128 weight_prev_squared_x4 =
      _mm_mul_ps(weight_prev_x4, weight_prev_x4);
  const __m128 weight_prev_4th_x4 =
      _mm_mul_ps(weight_prev_squared_x4, weight_prev_squared_x4);

  // Compute z[n], z[n-1], z[n-2], and z[n-3] in parallel in lanes 3, 2, 1 and
  // 0, respectively.
  __m128 max_x4 = _mm_setzero_ps();
  __m128 ewma_x4 = _mm_setr_ps(0.0f, 0.0f, 0.0f, initial_value);
  int i;
  for (i = 0; i < last_index; i += 4) {
    ewma_x4 = _mm_mul_ps(ewma_x4, weight_prev_4th_x4);
    const __m128 sample_x4 = _mm_load_ps(src + i);
    const __m128 sample_squared_x4 = _mm_mul_ps(sample_x4, sample_x4);
    max_x4 = _mm_max_ps(max_x4, sample_squared_x4);
    // Note: The compiler optimizes this to a single multiply-and-accumulate
    // instruction:
    ewma_x4 = _mm_add_ps(ewma_x4,
                         _mm_mul_ps(sample_squared_x4, smoothing_factor_x4));
  }

  // y[n] = z[n] + (1-a)^1(z[n-1]) + (1-a)^2(z[n-2]) + (1-a)^3(z[n-3])
  float ewma = EXTRACT_FLOAT(ewma_x4, 3);
  ewma_x4 = _mm_mul_ps(ewma_x4, weight_prev_x4);
  ewma += EXTRACT_FLOAT(ewma_x4, 2);
  ewma_x4 = _mm_mul_ps(ewma_x4, weight_prev_x4);
  ewma += EXTRACT_FLOAT(ewma_x4, 1);
  ewma_x4 = _mm_mul_ss(ewma_x4, weight_prev_x4);
  ewma += EXTRACT_FLOAT(ewma_x4, 0);

  // Fold the maximums together to get the overall maximum.
  max_x4 = _mm_max_ps(max_x4,
                      _mm_shuffle_ps(max_x4, max_x4, _MM_SHUFFLE(3, 3, 1, 1)));
  max_x4 = _mm_max_ss(max_x4, _mm_shuffle_ps(max_x4, max_x4, 2));

  std::pair<float, float> result(ewma, EXTRACT_FLOAT(max_x4, 0));

  // Handle remaining values at the end of |src|.
  for (; i < len; ++i) {
    result.first *= weight_prev;
    const float sample = src[i];
    const float sample_squared = sample * sample;
    result.first += sample_squared * smoothing_factor;
    result.second = std::max(result.second, sample_squared);
  }

  return result;
}

__attribute__((target("avx2,fma"))) std::pair<float, float>
EWMAAndMaxPower_AVX2(float initial_value,
                     const float src[],
                     int len,
                     float smoothing_factor) {
  const int rem = len % 8;
  const int last_index = len - rem;
  const float weight_prev = 1.0f - smoothing_factor;

  // y[7] = a(S[7]^2) + a(1-a)(S[6]^2) + a(1-a)^2(S[5]^2) + a(1-a)^3(S[4]^2) +
  //        a(1-a)^4(S[3]^2) + a(1-a)^5(S[2]^2) + a(1-a)^6(S[1]^2) +
  //        a(1-a)^7(S[0]^2) + (1-a)^8 * y[-1].
  //      = SumS[0-7] + (1-a)^8 * y[-1].
  // y[15] = SumS[8-15] + (1-a)^8 * y[7].
  // y[23] = SumS[16-23] + (1-a)^8 * y[15].
  // ......
  // So the strategy is to read 8 float data at a time, and then
  // calculate the average power and the maximum squared element value.
  const __m256 sum_coeff =
      !weight_prev
          ? _mm256_set_ps(smoothing_factor, 0, 0, 0, 0, 0, 0, 0)
          : _mm256_set_ps(smoothing_factor, smoothing_factor * weight_prev,
                          smoothing_factor * std::pow(weight_prev, 2),
                          smoothing_factor * std::pow(weight_prev, 3),
                          smoothing_factor * std::pow(weight_prev, 4),
                          smoothing_factor * std::pow(weight_prev, 5),
                          smoothing_factor * std::pow(weight_prev, 6),
                          smoothing_factor * std::pow(weight_prev, 7));

  __m256 max = _mm256_setzero_ps();
  __m256 res = _mm256_set_ps(initial_value, 0, 0, 0, 0, 0, 0, 0);
  __m256 res_coeff = !weight_prev ? _mm256_set1_ps(0)
                                  : _mm256_set1_ps(std::pow(weight_prev, 8));
  bool aligned_src = (reinterpret_cast<uintptr_t>(src) & 0x1F) == 0;
  int i = 0;
  for (; i < last_index; i += 8) {
    __m256 sample =
        aligned_src ? _mm256_load_ps(src + i) : _mm256_loadu_ps(src + i);
    __m256 sample_x2 = _mm256_mul_ps(sample, sample);
    max = _mm256_max_ps(max, sample_x2);
    res = _mm256_fmadd_ps(sample_x2, sum_coeff, _mm256_mul_ps(res, res_coeff));
  }

  std::pair<float, float> result(initial_value, 0);
  // Sum components together to get the average power.
  __m128 m128_sums =
      _mm_add_ps(_mm256_extractf128_ps(res, 0), _mm256_extractf128_ps(res, 1));
  m128_sums = _mm_add_ps(_mm_movehl_ps(m128_sums, m128_sums), m128_sums);
  float res_sum;
  _mm_store_ss(&res_sum,
               _mm_add_ss(m128_sums, _mm_shuffle_ps(m128_sums, m128_sums, 1)));
  result.first = res_sum;

  __m128 m128_max = _mm_setzero_ps();
  m128_max =
      _mm_max_ps(_mm256_extractf128_ps(max, 0), _mm256_extractf128_ps(max, 1));
  m128_max = _mm_max_ps(
      m128_max, _mm_shuffle_ps(m128_max, m128_max, _MM_SHUFFLE(3, 3, 1, 1)));
  m128_max = _mm_max_ss(m128_max, _mm_shuffle_ps(m128_max, m128_max, 2));
  result.second = std::max(EXTRACT_FLOAT(m128_max, 0), result.second);

  // Handle remaining values at the end of |src|.
  for (; i < len; ++i) {
    result.first *= weight_prev;
    const float sample = src[i];
    const float sample_squared = sample * sample;
    result.first += sample_squared * smoothing_factor;
    result.second = std::max(result.second, sample_squared);
  }

  return result;
}
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
void FMAC_NEON(const float src[], float scale, int len, float dest[]) {
  const int rem = len % 4;
  const int last_index = len - rem;
  float32x4_t m_scale = vmovq_n_f32(scale);
  for (int i = 0; i < last_index; i += 4) {
    vst1q_f32(dest + i, vmlaq_f32(
        vld1q_f32(dest + i), vld1q_f32(src + i), m_scale));
  }

  // Handle any remaining values that wouldn't fit in an NEON pass.
  for (int i = last_index; i < len; ++i)
    dest[i] += src[i] * scale;
}

void FMUL_NEON(const float src[], float scale, int len, float dest[]) {
  const int rem = len % 4;
  const int last_index = len - rem;
  float32x4_t m_scale = vmovq_n_f32(scale);
  for (int i = 0; i < last_index; i += 4)
    vst1q_f32(dest + i, vmulq_f32(vld1q_f32(src + i), m_scale));

  // Handle any remaining values that wouldn't fit in an NEON pass.
  for (int i = last_index; i < len; ++i)
    dest[i] = src[i] * scale;
}

std::pair<float, float> EWMAAndMaxPower_NEON(
    float initial_value, const float src[], int len, float smoothing_factor) {
  // When the recurrence is unrolled, we see that we can split it into 4
  // separate lanes of evaluation:
  //
  // y[n] = a(S[n]^2) + (1-a)(y[n-1])
  //      = a(S[n]^2) + (1-a)^1(aS[n-1]^2) + (1-a)^2(aS[n-2]^2) + ...
  //      = z[n] + (1-a)^1(z[n-1]) + (1-a)^2(z[n-2]) + (1-a)^3(z[n-3])
  //
  // where z[n] = a(S[n]^2) + (1-a)^4(z[n-4]) + (1-a)^8(z[n-8]) + ...
  //
  // Thus, the strategy here is to compute z[n], z[n-1], z[n-2], and z[n-3] in
  // each of the 4 lanes, and then combine them to give y[n].

  const int rem = len % 4;
  const int last_index = len - rem;

  const float32x4_t smoothing_factor_x4 = vdupq_n_f32(smoothing_factor);
  const float weight_prev = 1.0f - smoothing_factor;
  const float32x4_t weight_prev_x4 = vdupq_n_f32(weight_prev);
  const float32x4_t weight_prev_squared_x4 =
      vmulq_f32(weight_prev_x4, weight_prev_x4);
  const float32x4_t weight_prev_4th_x4 =
      vmulq_f32(weight_prev_squared_x4, weight_prev_squared_x4);

  // Compute z[n], z[n-1], z[n-2], and z[n-3] in parallel in lanes 3, 2, 1 and
  // 0, respectively.
  float32x4_t max_x4 = vdupq_n_f32(0.0f);
  float32x4_t ewma_x4 = vsetq_lane_f32(initial_value, vdupq_n_f32(0.0f), 3);
  int i;
  for (i = 0; i < last_index; i += 4) {
    ewma_x4 = vmulq_f32(ewma_x4, weight_prev_4th_x4);
    const float32x4_t sample_x4 = vld1q_f32(src + i);
    const float32x4_t sample_squared_x4 = vmulq_f32(sample_x4, sample_x4);
    max_x4 = vmaxq_f32(max_x4, sample_squared_x4);
    ewma_x4 = vmlaq_f32(ewma_x4, sample_squared_x4, smoothing_factor_x4);
  }

  // y[n] = z[n] + (1-a)^1(z[n-1]) + (1-a)^2(z[n-2]) + (1-a)^3(z[n-3])
  float ewma = vgetq_lane_f32(ewma_x4, 3);
  ewma_x4 = vmulq_f32(ewma_x4, weight_prev_x4);
  ewma += vgetq_lane_f32(ewma_x4, 2);
  ewma_x4 = vmulq_f32(ewma_x4, weight_prev_x4);
  ewma += vgetq_lane_f32(ewma_x4, 1);
  ewma_x4 = vmulq_f32(ewma_x4, weight_prev_x4);
  ewma += vgetq_lane_f32(ewma_x4, 0);

  // Fold the maximums together to get the overall maximum.
  float32x2_t max_x2 = vpmax_f32(vget_low_f32(max_x4), vget_high_f32(max_x4));
  max_x2 = vpmax_f32(max_x2, max_x2);

  std::pair<float, float> result(ewma, vget_lane_f32(max_x2, 0));

  // Handle remaining values at the end of |src|.
  for (; i < len; ++i) {
    result.first *= weight_prev;
    const float sample = src[i];
    const float sample_squared = sample * sample;
    result.first += sample_squared * smoothing_factor;
    result.second = std::max(result.second, sample_squared);
  }

  return result;
}
#endif

}  // namespace vector_math
}  // namespace media
