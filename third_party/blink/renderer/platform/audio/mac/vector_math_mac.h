// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_MAC_VECTOR_MATH_MAC_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_MAC_VECTOR_MATH_MAC_H_

#include <Accelerate/Accelerate.h>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"

namespace blink {
namespace vector_math {
namespace mac {

// On the Mac we use the highly optimized versions in Accelerate.framework
// In 32-bit mode (__ppc__ or __i386__) <Accelerate/Accelerate.h> includes
// <vecLib/vDSP_translate.h> which defines macros of the same name as
// our namespaced function names, so we must handle this case differently. Other
// architectures (64bit, ARM, etc.) do not include this header file.

ALWAYS_INLINE static void Conv(base::span<const float> source,
                               base::span<const float> filter,
                               base::span<float> dest,
                               base::span<const float> /*prepared_filter*/) {
  DCHECK_GT(filter.size(), 0u);
  DCHECK_GE(source.size(), dest.size() + filter.size() - 1u);
  const float* filter_p = &filter.back();
#if defined(ARCH_CPU_X86)
  ::conv(source.data(), 1, filter_p, -1, dest.data(), 1, dest.size(),
         filter.size());
#else
  vDSP_conv(source.data(), 1, filter_p, -1, dest.data(), 1, dest.size(),
            filter.size());
#endif
}

ALWAYS_INLINE static void Vadd(base::span<const float> source1,
                               base::span<const float> source2,
                               base::span<float> dest) {
  DCHECK_EQ(source1.size(), dest.size());
  DCHECK_EQ(source2.size(), dest.size());
#if defined(ARCH_CPU_X86)
  ::vadd(source1.data(), 1, source2.data(), 1, dest.data(), 1, dest.size());
#else
  vDSP_vadd(source1.data(), 1, source2.data(), 1, dest.data(), 1, dest.size());
#endif
}

ALWAYS_INLINE static void Vsub(base::span<const float> source1,
                               base::span<const float> source2,
                               base::span<float> dest) {
  DCHECK_EQ(source1.size(), dest.size());
  DCHECK_EQ(source2.size(), dest.size());
  // NOTE: We define Vsub to be source1 - source2. The vDSP routines
  // do source2 - source1, so swap the args when calling the vDSP
  // routines.
#if defined(ARCH_CPU_X86)
  ::vsub(source2.data(), 1, source1.data(), 1, dest.data(), 1, dest.size());
#else
  vDSP_vsub(source2.data(), 1, source1.data(), 1, dest.data(), 1, dest.size());
#endif
}

ALWAYS_INLINE static void Vclip(base::span<const float> source,
                                float low_threshold,
                                float high_threshold,
                                base::span<float> dest) {
  DCHECK_EQ(source.size(), dest.size());
  vDSP_vclip(source.data(), 1, &low_threshold, &high_threshold, dest.data(), 1,
             dest.size());
}

ALWAYS_INLINE static float Vmaxmgv(base::span<const float> source) {
  float max = 0;
  vDSP_maxmgv(source.data(), 1, &max, source.size());
  return max;
}

ALWAYS_INLINE static void Vmul(base::span<const float> source1,
                               base::span<const float> source2,
                               base::span<float> dest) {
  DCHECK_EQ(source1.size(), dest.size());
  DCHECK_EQ(source2.size(), dest.size());
#if defined(ARCH_CPU_X86)
  ::vmul(source1.data(), 1, source2.data(), 1, dest.data(), 1, dest.size());
#else
  vDSP_vmul(source1.data(), 1, source2.data(), 1, dest.data(), 1, dest.size());
#endif
}

ALWAYS_INLINE static void Vsma(base::span<const float> source,
                               float scale,
                               base::span<float> dest) {
  DCHECK_EQ(source.size(), dest.size());
  vDSP_vsma(source.data(), 1, &scale, dest.data(), 1, dest.data(), 1,
            dest.size());
}

ALWAYS_INLINE static void Vsmul(base::span<const float> source,
                                float scale,
                                base::span<float> dest) {
  DCHECK_EQ(source.size(), dest.size());
#if defined(ARCH_CPU_X86)
  ::vsmul(source.data(), 1, &scale, dest.data(), 1, dest.size());
#else
  vDSP_vsmul(source.data(), 1, &scale, dest.data(), 1, dest.size());
#endif
}

ALWAYS_INLINE static void Vsadd(base::span<const float> source,
                                float addend,
                                base::span<float> dest) {
  DCHECK_EQ(source.size(), dest.size());
#if defined(ARCH_CPU_X86)
  ::vsadd(source.data(), 1, &addend, dest.data(), 1, dest.size());
#else
  vDSP_vsadd(source.data(), 1, &addend, dest.data(), 1, dest.size());
#endif
}

ALWAYS_INLINE static float Vsvesq(base::span<const float> source) {
  float sum = 0;
  vDSP_svesq(source.data(), 1, &sum, source.size());
  return sum;
}

ALWAYS_INLINE static void Zvmul(base::span<const float> real1,
                                base::span<const float> imag1,
                                base::span<const float> real2,
                                base::span<const float> imag2,
                                base::span<float> real_dest,
                                base::span<float> imag_dest) {
  DCHECK_EQ(real1.size(), real_dest.size());
  DCHECK_EQ(imag1.size(), real_dest.size());
  DCHECK_EQ(real2.size(), real_dest.size());
  DCHECK_EQ(imag2.size(), real_dest.size());
  DCHECK_EQ(imag_dest.size(), real_dest.size());
  DSPSplitComplex sc1;
  DSPSplitComplex sc2;
  DSPSplitComplex dest;
  sc1.realp = const_cast<float*>(real1.data());
  sc1.imagp = const_cast<float*>(imag1.data());
  sc2.realp = const_cast<float*>(real2.data());
  sc2.imagp = const_cast<float*>(imag2.data());
  dest.realp = real_dest.data();
  dest.imagp = imag_dest.data();
#if defined(ARCH_CPU_X86)
  ::zvmul(&sc1, 1, &sc2, 1, &dest, 1, real_dest.size(), 1);
#else
  vDSP_zvmul(&sc1, 1, &sc2, 1, &dest, 1, real_dest.size(), 1);
#endif
}

}  // namespace mac
}  // namespace vector_math
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_MAC_VECTOR_MATH_MAC_H_
