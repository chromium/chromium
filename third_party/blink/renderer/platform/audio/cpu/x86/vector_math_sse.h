// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_SSE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_SSE_H_

#include <cstddef>

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"

namespace blink {
namespace vector_math {
namespace sse {

constexpr size_t kBitsPerRegister = 128u;
constexpr size_t kPackedFloatsPerRegister =
    kBitsPerRegister / (sizeof(float) * 8u);
constexpr size_t kFramesToProcessMask = ~(kPackedFloatsPerRegister - 1u);

bool IsAligned(const float*);

// Direct vector convolution:
// dest[k] = sum(source[k+m]*filter[filter.size()-1-m]) for all m
// provided that |prepared_filter| is prepared with |PrepareFilterForConv|.
void Conv(base::span<const float> source,
          base::span<const float> prepared_filter,
          base::span<float> dest,
          size_t filter_size);

void PrepareFilterForConv(base::span<const float> filter,
                          AudioFloatArray* prepared_filter);

// dest[k] = source1[k] + source2[k]
void Vadd(base::span<const float> source1,
          base::span<const float> source2,
          base::span<float> dest);

// dest[k] = source1[k] - source2[k]
void Vsub(base::span<const float> source1,
          base::span<const float> source2,
          base::span<float> dest);

// dest[k] = clip(source[k], low_threshold, high_threshold)
//         = max(low_threshold, min(high_threshold, source[k]))
void Vclip(base::span<const float> source,
           float low_threshold,
           float high_threshold,
           base::span<float> dest);

// max = max(abs(source[k])) for all k
float Vmaxmgv(base::span<const float> source);

// dest[k] = source1[k] * source2[k]
void Vmul(base::span<const float> source1,
          base::span<const float> source2,
          base::span<float> dest);

// dest[k] += scale * source[k]
void Vsma(base::span<const float> source, float scale, base::span<float> dest);

// dest[k] = scale * source[k]
void Vsmul(base::span<const float> source, float scale, base::span<float> dest);

// dest[k] = addend + source[k]
void Vsadd(base::span<const float> source,
           float addend,
           base::span<float> dest);

// sum = sum(source[k]^2) for all k
float Vsvesq(base::span<const float> source);

// real_dest[k] = real1[k] * real2[k] - imag1[k] * imag2[k]
// imag_dest[k] = real1[k] * imag2[k] + imag1[k] * real2[k]
void Zvmul(base::span<const float> real1,
           base::span<const float> imag1,
           base::span<const float> real2,
           base::span<const float> imag2,
           base::span<float> real_dest,
           base::span<float> imag_dest);

}  // namespace sse
}  // namespace vector_math
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_SSE_H_
