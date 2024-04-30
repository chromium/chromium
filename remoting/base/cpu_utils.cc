// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/cpu_utils.h"

#include "build/build_config.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include "base/cpu.h"
#endif

namespace remoting {

namespace {

// Supporting SSE3 is a requirement for Chromium on x86/x64 so that is our base
// alignment. Both SSE3 (x86) and NEON (ARM) benefit from 16 byte alignment in
// libyuv so make that the default. If the CPU supports AVX2, then we will use
// that alignment instead. In the cases where AVX512 is used in libyuv, the
// alignment requirements are the same as for AVX2.
constexpr int kDefaultAlignmentBytes = 16;
#if defined(ARCH_CPU_X86_FAMILY)
constexpr int kAvx2AlignmentBytes = 32;
#endif

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
  // We only need to calculate this once since the processor capabilities won't
  // change while the process is running.
  static const int alignment = []() {
#if defined(ARCH_CPU_X86_FAMILY)
    if (base::CPU().has_avx2()) {
      return kAvx2AlignmentBytes;
    }
#endif
    return kDefaultAlignmentBytes;
  }();

  return alignment;
}

}  // namespace remoting
