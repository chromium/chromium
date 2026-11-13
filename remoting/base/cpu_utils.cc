// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/cpu_utils.h"

#include "base/check_op.h"
#include "build/build_config.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include "base/cpu.h"
#endif

namespace remoting {

namespace {

// Supporting SSE3 is a requirement for Chromium on x86/x64 so 16 bytes is our
// baseline alignment (matching SSE3 on x86 and NEON on ARM). On x86 CPUs with
// AVX2 or AVX-512BW, use 32 or 64 respectively.
//
// Dual byte/pixel usage: Callers pass `GetSimdMemoryAlignment()` both as a byte
// alignment to `aom_img_alloc` / `vpx_img_alloc` (where 8-bit planar YUV stores
// 1 byte per luma/I444 sample) and as a horizontal pixel X-alignment in
// `GetRowAlignedRect()`. Aligning row widths to 32 or 64 pixels satisfies
// `IS_ALIGNED(width, 32)` / `IS_ALIGNED(width, 64)` in libyuv so
// `ARGBToYMatrixRow`, `ARGBToUVMatrixRow` (I420), and `ARGBToUV444MatrixRow`
// (I444) dispatch full AVX2 / AVX-512BW vector kernels directly instead of
// bouncing through `_Any` stack-buffer tail wrappers.
constexpr int kDefaultAlignmentBytes = 16;
#if defined(ARCH_CPU_X86_FAMILY)
constexpr int kAvx2AlignmentBytes = 32;
constexpr int kAvx512AlignmentBytes = 64;
#endif

int g_simd_alignment_for_testing = 0;

}  // namespace

bool IsCpuSupported() {
#if defined(ARCH_CPU_X86_FAMILY)
  // x86 Chromium builds target SSE3.
  // See crbug.com/1251642 for more info.
  if (!base::CPU().has_sse3()) {
    return false;
  }
#endif
  return true;
}

int GetSimdMemoryAlignment() {
  if (g_simd_alignment_for_testing > 0) [[unlikely]] {
    return g_simd_alignment_for_testing;
  }

  // We only need to calculate this once since the processor capabilities won't
  // change while the process is running.
  static const int alignment = []() {
#if defined(ARCH_CPU_X86_FAMILY)
    base::CPU cpu;
    if (cpu.has_avx512_bw()) {
      return kAvx512AlignmentBytes;
    }
    if (cpu.has_avx2()) {
      return kAvx2AlignmentBytes;
    }
#endif
    return kDefaultAlignmentBytes;
  }();

  return alignment;
}

ScopedSimdMemoryAlignmentForTesting::ScopedSimdMemoryAlignmentForTesting(
    int alignment)
    : previous_alignment_(g_simd_alignment_for_testing) {
  CHECK_GT(alignment, 0);
  CHECK_EQ(alignment & (alignment - 1), 0);
  g_simd_alignment_for_testing = alignment;
}

ScopedSimdMemoryAlignmentForTesting::~ScopedSimdMemoryAlignmentForTesting() {
  g_simd_alignment_for_testing = previous_alignment_;
}

}  // namespace remoting
