// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(ARCH_CPU_X86_FAMILY) && !BUILDFLAG(IS_MAC)

#include "third_party/blink/renderer/platform/audio/cpu/x86/vector_math_sse.h"

#include <xmmintrin.h>

namespace blink {
namespace vector_math {
namespace sse {

using MType = __m128;

}  // namespace sse
}  // namespace vector_math
}  // namespace blink

#define MM_PS(name) _mm_##name##_ps
#define VECTOR_MATH_SIMD_NAMESPACE_NAME sse

#include "third_party/blink/renderer/platform/audio/cpu/x86/vector_math_impl.h"

#undef MM_PS
#undef VECTOR_MATH_SIMD_NAMESPACE_NAME

#endif  // defined(ARCH_CPU_X86_FAMILY) && !BUILDFLAG(IS_MAC)
