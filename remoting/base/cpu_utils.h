// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CPU_UTILS_H_
#define REMOTING_BASE_CPU_UTILS_H_

namespace remoting {

// Returns true if the CPU meets the minimum hardware requirements (e.g. it
// supports a minimum required instruction set), otherwise false.
bool IsCpuSupported();

// Returns the alignment (16, 32, or 64) required to use the most efficient SIMD
// instructions (SSE3, AVX2, AVX-512BW, NEON, etc.) supported by the processor.
// Callers use this both as a byte-stride alignment for 8-bit planar YUV image
// allocations and as a horizontal pixel X-alignment in `GetRowAlignedRect()`.
int GetSimdMemoryAlignment();

// Overrides the value returned by `GetSimdMemoryAlignment()` for the lifetime
// of this object so unit tests can exercise 16-, 32-, and 64-byte alignment
// paths regardless of the host CPU's SIMD capabilities. `alignment` must be a
// positive power of two.
class ScopedSimdMemoryAlignmentForTesting {
 public:
  explicit ScopedSimdMemoryAlignmentForTesting(int alignment);
  ScopedSimdMemoryAlignmentForTesting(
      const ScopedSimdMemoryAlignmentForTesting&) = delete;
  ScopedSimdMemoryAlignmentForTesting& operator=(
      const ScopedSimdMemoryAlignmentForTesting&) = delete;
  ~ScopedSimdMemoryAlignmentForTesting();

 private:
  const int previous_alignment_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CPU_UTILS_H_
