// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included from
// vector_math_avx.h and from vector_math_sse.h with different macro
// definitions. The following line silences a presubmit warning that would
// otherwise be triggered by this: no-include-guard-because-multiply-included

#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(ARCH_CPU_X86_FAMILY) && !BUILDFLAG(IS_MAC)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"

namespace blink {
namespace vector_math {
namespace VECTOR_MATH_SIMD_NAMESPACE_NAME {

// This stride is chosen so that the same prepared filter created by
// AVX::PrepareFilterForConv can be used by both AVX::Conv and sse::Conv.
// A prepared filter created by sse::PrepareFilterForConv can only be used
// by sse::Conv.
constexpr size_t kReversedFilterStride = 8u / kPackedFloatsPerRegister;

bool IsAligned(const float* p) {
  constexpr size_t kBytesPerRegister = kBitsPerRegister / 8u;
  constexpr size_t kAlignmentOffsetMask = kBytesPerRegister - 1u;
  return (reinterpret_cast<size_t>(p) & kAlignmentOffsetMask) == 0u;
}

void PrepareFilterForConv(const float* filter_p,
                          size_t filter_size,
                          AudioFloatArray* prepared_filter) {
  // Only contiguous convolution is implemented.
  DCHECK(prepared_filter);

  // Reverse the filter and repeat each value across a vector
  prepared_filter->Allocate(kReversedFilterStride * kPackedFloatsPerRegister *
                            filter_size);
  MType* reversed_filter = reinterpret_cast<MType*>(prepared_filter->Data());
  for (size_t i = 0; i < filter_size; ++i) {
    UNSAFE_TODO(reversed_filter[kReversedFilterStride * i]) =
        MM_PS(set1)(*(UNSAFE_TODO(filter_p - i)));
  }
}

// Direct vector convolution:
// dest[k] = sum(source[k+m]*filter[m*filter_stride]) for all m
// provided that |prepared_filter_p| is |prepared_filter->Data()| and that
// |prepared_filter| is prepared with |PrepareFilterForConv|.
void Conv(const float* source_p,
          const float* prepared_filter_p,
          float* dest_p,
          size_t frames_to_process,
          size_t filter_size) {
  const float* const dest_end_p = UNSAFE_TODO(dest_p + frames_to_process);

  DCHECK_EQ(0u, frames_to_process % kPackedFloatsPerRegister);
  DCHECK_EQ(0u, filter_size % kPackedFloatsPerRegister);

  const MType* reversed_filter =
      reinterpret_cast<const MType*>(prepared_filter_p);

  // Do convolution with kPackedFloatsPerRegister inputs at a time.
  while (dest_p < dest_end_p) {
    MType m_convolution_sum = MM_PS(setzero)();

    // |filter_size| is a multiple of kPackedFloatsPerRegister so we can unroll
    // the loop by kPackedFloatsPerRegister, manually.
    for (size_t i = 0; i < filter_size; i += kPackedFloatsPerRegister) {
      for (size_t j = 0; j < kPackedFloatsPerRegister; ++j) {
        size_t k = i + j;
        MType m_product;
        MType m_source;

        m_source = MM_PS(loadu)(UNSAFE_TODO(source_p + k));
        m_product = MM_PS(mul)(
            UNSAFE_TODO(reversed_filter[kReversedFilterStride * k]), m_source);
        m_convolution_sum = MM_PS(add)(m_convolution_sum, m_product);
      }
    }
    MM_PS(storeu)(dest_p, m_convolution_sum);

    UNSAFE_TODO(source_p += kPackedFloatsPerRegister);
    UNSAFE_TODO(dest_p += kPackedFloatsPerRegister);
  }
}

// dest[k] = source1[k] + source2[k]
void Vadd(base::span<const float> source1,
          base::span<const float> source2,
          base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source1.size(), dest.size());
  CHECK_EQ(source2.size(), dest.size());
  DCHECK(IsAligned(source1.data()));
  DCHECK_EQ(0u, dest.size() % kPackedFloatsPerRegister);

#define ADD_ALL(loadSource2, storeDest)                                   \
  for (size_t i = 0; i < dest.size(); i += kPackedFloatsPerRegister) {    \
    MType m_source1 =                                                     \
        MM_PS(load)(source1.subspan(i, kPackedFloatsPerRegister).data()); \
    MType m_source2 = MM_PS(loadSource2)(                                 \
        source2.subspan(i, kPackedFloatsPerRegister).data());             \
    MType m_dest = MM_PS(add)(m_source1, m_source2);                      \
    MM_PS(storeDest)(dest.subspan(i, kPackedFloatsPerRegister).data(),    \
                     m_dest);                                             \
  }

  const float* dest_p = dest.data();
  if (IsAligned(source2.data())) {
    if (IsAligned(dest_p)) {
      ADD_ALL(load, store);
    } else {
      ADD_ALL(load, storeu);
    }
  } else {
    if (IsAligned(dest_p)) {
      ADD_ALL(loadu, store);
    } else {
      ADD_ALL(loadu, storeu);
    }
  }
#undef ADD_ALL
}

void Vsub(base::span<const float> source1,
          base::span<const float> source2,
          base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source1.size(), dest.size());
  CHECK_EQ(source2.size(), dest.size());
  DCHECK(IsAligned(source1.data()));
  DCHECK_EQ(0u, dest.size() % kPackedFloatsPerRegister);

#define SUB_ALL(loadSource2, storeDest)                                   \
  for (size_t i = 0; i < dest.size(); i += kPackedFloatsPerRegister) {    \
    MType m_source1 =                                                     \
        MM_PS(load)(source1.subspan(i, kPackedFloatsPerRegister).data()); \
    MType m_source2 = MM_PS(loadSource2)(                                 \
        source2.subspan(i, kPackedFloatsPerRegister).data());             \
    MType m_dest = MM_PS(sub)(m_source1, m_source2);                      \
    MM_PS(storeDest)(dest.subspan(i, kPackedFloatsPerRegister).data(),    \
                     m_dest);                                             \
  }

  const float* dest_p = dest.data();
  if (IsAligned(source2.data())) {
    if (IsAligned(dest_p)) {
      SUB_ALL(load, store);
    } else {
      SUB_ALL(load, storeu);
    }
  } else {
    if (IsAligned(dest_p)) {
      SUB_ALL(loadu, store);
    } else {
      SUB_ALL(loadu, storeu);
    }
  }
#undef SUB_ALL
}

// dest[k] = clip(source[k], low_threshold, high_threshold)
//         = max(low_threshold, min(high_threshold, source[k]))
void Vclip(base::span<const float> source,
           float low_threshold,
           float high_threshold,
           base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source.size(), dest.size());
  DCHECK(IsAligned(source.data()));
  DCHECK_EQ(0u, dest.size() % kPackedFloatsPerRegister);

  MType m_low_threshold = MM_PS(set1)(low_threshold);
  MType m_high_threshold = MM_PS(set1)(high_threshold);

#define CLIP_ALL(storeDest)                                                  \
  for (size_t i = 0; i < dest.size(); i += kPackedFloatsPerRegister) {       \
    MType m_source =                                                         \
        MM_PS(load)(source.subspan(i, kPackedFloatsPerRegister).data());     \
    MType m_dest =                                                           \
        MM_PS(max)(m_low_threshold, MM_PS(min)(m_high_threshold, m_source)); \
    MM_PS(storeDest)(dest.subspan(i, kPackedFloatsPerRegister).data(),       \
                     m_dest);                                                \
  }

  if (IsAligned(dest.data())) {
    CLIP_ALL(store);
  } else {
    CLIP_ALL(storeu);
  }

#undef CLIP_ALL
}

// max = max(abs(source[k])) for all k
float Vmaxmgv(base::span<const float> source) {
  DCHECK(IsAligned(source.data()));
  DCHECK_EQ(0u, source.size() % kPackedFloatsPerRegister);

  constexpr uint32_t kMask = 0x7FFFFFFFu;
  const float kMask_float = base::bit_cast<float>(kMask);

  MType m_mask = MM_PS(set1)(kMask_float);
  MType m_max = MM_PS(setzero)();

  if (source.size() >= kPackedFloatsPerRegister) {
    const size_t end = source.size() - kPackedFloatsPerRegister;
    for (size_t i = 0; i <= end; i += kPackedFloatsPerRegister) {
      MType m_source =
          MM_PS(load)(source.subspan(i, kPackedFloatsPerRegister).data());
      // Calculate the absolute value by ANDing the source with the mask,
      // which will set the sign bit to 0.
      m_source = MM_PS (and)(m_source, m_mask);
      m_max = MM_PS(max)(m_source, m_max);
    }
  }

  // Combine the packed floats.
  alignas(alignof(MType)) std::array<float, kPackedFloatsPerRegister> maxes;
  MM_PS(store)(maxes.data(), m_max);
  float max = 0;
  for (float max_val : maxes) {
    max = std::max(max, max_val);
  }
  return max;
}

// dest[k] = source1[k] * source2[k]
void Vmul(base::span<const float> source1,
          base::span<const float> source2,
          base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source1.size(), dest.size());
  CHECK_EQ(source2.size(), dest.size());
  DCHECK(IsAligned(source1.data()));
  DCHECK_EQ(0u, dest.size() % kPackedFloatsPerRegister);

#define MULTIPLY_ALL(loadSource2, storeDest)                              \
  for (size_t i = 0; i < dest.size(); i += kPackedFloatsPerRegister) {    \
    MType m_source1 =                                                     \
        MM_PS(load)(source1.subspan(i, kPackedFloatsPerRegister).data()); \
    MType m_source2 = MM_PS(loadSource2)(                                 \
        source2.subspan(i, kPackedFloatsPerRegister).data());             \
    MType m_dest = MM_PS(mul)(m_source1, m_source2);                      \
    MM_PS(storeDest)(dest.subspan(i, kPackedFloatsPerRegister).data(),    \
                     m_dest);                                             \
  }

  const float* dest_p = dest.data();
  if (IsAligned(source2.data())) {
    if (IsAligned(dest_p)) {
      MULTIPLY_ALL(load, store);
    } else {
      MULTIPLY_ALL(load, storeu);
    }
  } else {
    if (IsAligned(dest_p)) {
      MULTIPLY_ALL(loadu, store);
    } else {
      MULTIPLY_ALL(loadu, storeu);
    }
  }
#undef MULTIPLY_ALL
}

// dest[k] += scale * source[k]
void Vsma(base::span<const float> source, float scale, base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source.size(), dest.size());
  DCHECK(IsAligned(source.data()));
  DCHECK_EQ(0u, dest.size() % kPackedFloatsPerRegister);

  const MType m_scale = MM_PS(set1)(scale);

#define SCALAR_MULTIPLY_AND_ADD_ALL(loadDest, storeDest)                   \
  for (size_t i = 0; i < dest.size(); i += kPackedFloatsPerRegister) {     \
    MType m_source =                                                       \
        MM_PS(load)(source.subspan(i, kPackedFloatsPerRegister).data());   \
    MType m_dest =                                                         \
        MM_PS(loadDest)(dest.subspan(i, kPackedFloatsPerRegister).data()); \
    m_dest = MM_PS(add)(m_dest, MM_PS(mul)(m_scale, m_source));            \
    MM_PS(storeDest)(dest.subspan(i, kPackedFloatsPerRegister).data(),     \
                     m_dest);                                              \
  }

  if (IsAligned(dest.data())) {
    SCALAR_MULTIPLY_AND_ADD_ALL(load, store);
  } else {
    SCALAR_MULTIPLY_AND_ADD_ALL(loadu, storeu);
  }

#undef SCALAR_MULTIPLY_AND_ADD_ALL
}

// dest[k] = scale * source[k]
void Vsmul(base::span<const float> source,
           float scale,
           base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source.size(), dest.size());
  DCHECK(IsAligned(source.data()));
  DCHECK_EQ(0u, dest.size() % kPackedFloatsPerRegister);

  const MType m_scale = MM_PS(set1)(scale);

#define SCALAR_MULTIPLY_ALL(storeDest)                                   \
  for (size_t i = 0; i < dest.size(); i += kPackedFloatsPerRegister) {   \
    MType m_source =                                                     \
        MM_PS(load)(source.subspan(i, kPackedFloatsPerRegister).data()); \
    MType m_dest = MM_PS(mul)(m_scale, m_source);                        \
    MM_PS(storeDest)(dest.subspan(i, kPackedFloatsPerRegister).data(),   \
                     m_dest);                                            \
  }

  if (IsAligned(dest.data())) {
    SCALAR_MULTIPLY_ALL(store);
  } else {
    SCALAR_MULTIPLY_ALL(storeu);
  }

#undef SCALAR_MULTIPLY_ALL
}

// dest[k] = addend + source[k]
void Vsadd(base::span<const float> source,
           float addend,
           base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source.size(), dest.size());
  DCHECK(IsAligned(source.data()));
  DCHECK_EQ(0u, dest.size() % kPackedFloatsPerRegister);

  const MType m_addend = MM_PS(set1)(addend);

#define SCALAR_ADD_ALL(storeDest)                                        \
  for (size_t i = 0; i < dest.size(); i += kPackedFloatsPerRegister) {   \
    MType m_source =                                                     \
        MM_PS(load)(source.subspan(i, kPackedFloatsPerRegister).data()); \
    MType m_dest = MM_PS(add)(m_addend, m_source);                       \
    MM_PS(storeDest)(dest.subspan(i, kPackedFloatsPerRegister).data(),   \
                     m_dest);                                            \
  }

  if (IsAligned(dest.data())) {
    SCALAR_ADD_ALL(store);
  } else {
    SCALAR_ADD_ALL(storeu);
  }

#undef SCALAR_ADD_ALL
}

// sum = sum(source[k]^2) for all k
float Vsvesq(base::span<const float> source) {
  DCHECK(IsAligned(source.data()));
  DCHECK_EQ(0u, source.size() % kPackedFloatsPerRegister);

  MType m_sum = MM_PS(setzero)();

  if (source.size() >= kPackedFloatsPerRegister) {
    const size_t end = source.size() - kPackedFloatsPerRegister;
    for (size_t i = 0; i <= end; i += kPackedFloatsPerRegister) {
      MType m_source =
          MM_PS(load)(source.subspan(i, kPackedFloatsPerRegister).data());
      m_sum = MM_PS(add)(m_sum, MM_PS(mul)(m_source, m_source));
    }
  }

  // Combine the packed floats.
  alignas(alignof(MType)) std::array<float, kPackedFloatsPerRegister> sums;
  MM_PS(store)(sums.data(), m_sum);
  float sum = 0;
  for (float sum_val : sums) {
    sum += sum_val;
  }
  return sum;
}

// real_dest[k] = real1[k] * real2[k] - imag1[k] * imag2[k]
// imag_dest[k] = real1[k] * imag2[k] + imag1[k] * real2[k]
void Zvmul(base::span<const float> real1,
           base::span<const float> imag1,
           base::span<const float> real2,
           base::span<const float> imag2,
           base::span<float> real_dest,
           base::span<float> imag_dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(real1.size(), real_dest.size());
  CHECK_EQ(imag1.size(), real_dest.size());
  CHECK_EQ(real2.size(), real_dest.size());
  CHECK_EQ(imag2.size(), real_dest.size());
  CHECK_EQ(imag_dest.size(), real_dest.size());
  DCHECK(IsAligned(real1.data()));
  DCHECK_EQ(0u, real_dest.size() % kPackedFloatsPerRegister);

#define MULTIPLY_ALL(loadOtherThanReal1, storeDest)                         \
  for (size_t i = 0; i < real_dest.size(); i += kPackedFloatsPerRegister) { \
    MType m_real1 =                                                         \
        MM_PS(load)(real1.subspan(i, kPackedFloatsPerRegister).data());     \
    MType m_real2 = MM_PS(loadOtherThanReal1)(                              \
        real2.subspan(i, kPackedFloatsPerRegister).data());                 \
    MType m_imag1 = MM_PS(loadOtherThanReal1)(                              \
        imag1.subspan(i, kPackedFloatsPerRegister).data());                 \
    MType m_imag2 = MM_PS(loadOtherThanReal1)(                              \
        imag2.subspan(i, kPackedFloatsPerRegister).data());                 \
    MType m_real = MM_PS(sub)(MM_PS(mul)(m_real1, m_real2),                 \
                              MM_PS(mul)(m_imag1, m_imag2));                \
    MType m_imag = MM_PS(add)(MM_PS(mul)(m_real1, m_imag2),                 \
                              MM_PS(mul)(m_imag1, m_real2));                \
    MM_PS(storeDest)(real_dest.subspan(i, kPackedFloatsPerRegister).data(), \
                     m_real);                                               \
    MM_PS(storeDest)(imag_dest.subspan(i, kPackedFloatsPerRegister).data(), \
                     m_imag);                                               \
  }

  if (IsAligned(imag1.data()) && IsAligned(real2.data()) &&
      IsAligned(imag2.data()) && IsAligned(real_dest.data()) &&
      IsAligned(imag_dest.data())) {
    MULTIPLY_ALL(load, store);
  } else {
    MULTIPLY_ALL(loadu, storeu);
  }

#undef MULTIPLY_ALL
}

}  // namespace VECTOR_MATH_SIMD_NAMESPACE_NAME
}  // namespace vector_math
}  // namespace blink

#endif  // defined(ARCH_CPU_X86_FAMILY) && !BUILDFLAG(IS_MAC)
