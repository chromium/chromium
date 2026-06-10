/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/platform/audio/vector_math.h"

#include <cmath>

#include "base/compiler_specific.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "third_party/blink/renderer/platform/audio/mac/vector_math_mac.h"
#elif defined(CPU_ARM_NEON)
#include "third_party/blink/renderer/platform/audio/cpu/arm/vector_math_neon.h"
#elif defined(ARCH_CPU_X86_FAMILY)
#include "third_party/blink/renderer/platform/audio/cpu/x86/vector_math_x86.h"
#else
#include "third_party/blink/renderer/platform/audio/vector_math_scalar.h"
#endif

namespace blink::vector_math {

namespace {
#if BUILDFLAG(IS_MAC)
namespace impl = mac;
#elif defined(CPU_ARM_NEON)
namespace impl = neon;
#elif defined(ARCH_CPU_X86_FAMILY)
namespace impl = x86;
#else
namespace impl = scalar;
#endif
}  // namespace

void PrepareFilterForConv(base::span<const float> filter,
                          AudioFloatArray& prepared_filter) {
  // Only contiguous convolution is implemented by all implementations.
  // Correlation (positive |filter_stride|) and support for non-contiguous
  // vectors are not implemented by all implementations.
#if defined(ARCH_CPU_X86_FAMILY) && !BUILDFLAG(IS_MAC)
  const int filter_stride = -1;
  const float* filter_p = &filter.back();
  x86::PrepareFilterForConv(filter_p, filter_stride, filter.size(),
                            &prepared_filter);
#endif
}

void Conv(base::span<const float> source,
          base::span<const float> filter,
          base::span<float> dest,
          uint32_t frames_to_process,
          const AudioFloatArray& prepared_filter) {
  // Only contiguous convolution is implemented by all implementations.
  // Correlation (positive |filter_stride|) and support for non-contiguous
  // vectors are not implemented by all implementations.
  const int source_stride = 1;
  const int filter_stride = -1;
  const int dest_stride = 1;
  const float* filter_p = &filter.back();
  impl::Conv(source.data(), source_stride, filter_p, filter_stride, dest.data(),
             dest_stride, frames_to_process, filter.size(), &prepared_filter);
}

void Vadd(base::span<const float> source1,
          base::span<const float> source2,
          base::span<float> dest,
          uint32_t frames_to_process) {
  impl::Vadd(source1.first(frames_to_process), source2.first(frames_to_process),
             dest.first(frames_to_process));
}

void Vsub(base::span<const float> source1,
          base::span<const float> source2,
          base::span<float> dest,
          uint32_t frames_to_process) {
  impl::Vsub(source1.first(frames_to_process), source2.first(frames_to_process),
             dest.first(frames_to_process));
}

void Vclip(base::span<const float> source,
           float low_threshold,
           float high_threshold,
           base::span<float> dest) {
#if DCHECK_IS_ON()
  // Do the same DCHECKs that |ClampTo| would do so that optimization paths do
  // not have to do them.
  for (size_t i = 0u; i < dest.size(); ++i) {
    DCHECK(!std::isnan(source[i]));
  }
  // This also ensures that thresholds are not NaNs.
  DCHECK_LE(low_threshold, high_threshold);
#endif

  impl::Vclip(source.data(), 1, &low_threshold, &high_threshold, dest.data(), 1,
              base::checked_cast<uint32_t>(dest.size()));
}

void Vclip(base::span<const float> source,
           float low_threshold,
           float high_threshold,
           base::span<float> dest,
           uint32_t frames_to_process) {
#if DCHECK_IS_ON()
  // Do the same DCHECKs that |ClampTo| would do so that optimization paths do
  // not have to do them.
  for (size_t i = 0u; i < frames_to_process; ++i) {
    DCHECK(!std::isnan(source[i]));
  }
  // This also ensures that thresholds are not NaNs.
  DCHECK_LE(low_threshold, high_threshold);
#endif

  impl::Vclip(source.data(), 1, &low_threshold, &high_threshold, dest.data(), 1,
              frames_to_process);
}

float Vmaxmgv(base::span<const float> source, uint32_t frames_to_process) {
  float max = 0;

  impl::Vmaxmgv(source.data(), 1, &max, frames_to_process);

  return max;
}

void Vmul(base::span<const float> source1,
          base::span<const float> source2,
          base::span<float> dest,
          uint32_t frames_to_process) {
  impl::Vmul(source1.first(frames_to_process), source2.first(frames_to_process),
             dest.first(frames_to_process));
}

void Vsma(base::span<const float> source,
          float scale,
          base::span<float> dest,
          uint32_t frames_to_process) {
  impl::Vsma(source.data(), 1, &scale, dest.data(), 1, frames_to_process);
}

void Vsmul(base::span<const float> source,
           float scale,
           base::span<float> dest,
           uint32_t frames_to_process) {
  impl::Vsmul(source.data(), 1, &scale, dest.data(), 1, frames_to_process);
}

void Vsadd(base::span<const float> source,
           float addend,
           base::span<float> dest,
           uint32_t frames_to_process) {
  impl::Vsadd(source.data(), 1, &addend, dest.data(), 1, frames_to_process);
}

float Vsvesq(base::span<const float> source, uint32_t frames_to_process) {
  float sum = 0;

  impl::Vsvesq(source.data(), 1, &sum, frames_to_process);

  return sum;
}

void Zvmul(base::span<const float> real1,
           base::span<const float> imag1,
           base::span<const float> real2,
           base::span<const float> imag2,
           base::span<float> real_dest,
           base::span<float> imag_dest,
           uint32_t frames_to_process) {
  impl::Zvmul(real1.data(), imag1.data(), real2.data(), imag2.data(),
              real_dest.data(), imag_dest.data(), frames_to_process);
}

}  // namespace blink::vector_math
