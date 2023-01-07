/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_DENORMAL_DISABLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_DENORMAL_DISABLER_H_

#include <float.h>
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

// Deal with denormals. They can very seriously impact performance on x86.

// Define HAVE_DENORMAL if we support flushing denormals to zero.

#if BUILDFLAG(IS_WIN) && defined(COMPILER_MSVC)
// Windows compiled using MSVC with SSE2
#define HAVE_DENORMAL 1
#endif

#if defined(COMPILER_GCC) && defined(ARCH_CPU_X86_FAMILY)
// X86 chips can flush denormals
#define HAVE_DENORMAL 1
#endif

#if defined(ARCH_CPU_ARM_FAMILY)
#define HAVE_DENORMAL 1
#endif

#if defined(HAVE_DENORMAL)
class DenormalDisabler {
  DISALLOW_NEW();

 public:
  DenormalDisabler() { DisableDenormals(); }

  ~DenormalDisabler() { RestoreState(); }

  // This is a nop if we can flush denormals to zero in hardware.
  static inline float FlushDenormalFloatToZero(float f) { return f; }

 private:
  unsigned saved_csr_ = 0;

#if defined(COMPILER_GCC) && defined(ARCH_CPU_X86_FAMILY)
  inline void DisableDenormals() {
    saved_csr_ = GetCSR();
    SetCSR(saved_csr_ | 0x8040);
  }

  inline void RestoreState() { SetCSR(saved_csr_); }

  inline int GetCSR() {
    int result;
    asm volatile("stmxcsr %0" : "=m"(result));
    return result;
  }

  inline void SetCSR(int a) {
    int temp = a;
    asm volatile("ldmxcsr %0" : : "m"(temp));
  }

#elif BUILDFLAG(IS_WIN) && defined(COMPILER_MSVC)
  inline void DisableDenormals() {
    // Save the current state, and set mode to flush denormals.
    //
    // http://stackoverflow.com/questions/637175/possible-bug-in-controlfp-s-may-not-restore-control-word-correctly
    _controlfp_s(&saved_csr_, 0, 0);
    unsigned unused;
    _controlfp_s(&unused, _DN_FLUSH, _MCW_DN);
  }

  inline void RestoreState() {
    unsigned unused;
    _controlfp_s(&unused, saved_csr_, _MCW_DN);
  }
#elif defined(ARCH_CPU_ARM_FAMILY)
  inline void DisableDenormals() {
    saved_csr_ = GetStatusWord();
    // Bit 24 is the flush-to-zero mode control bit. Setting it to 1 flushes
    // denormals to 0.
    SetStatusWord(saved_csr_ | (1 << 24));
  }

  inline void RestoreState() { SetStatusWord(saved_csr_); }

  inline int GetStatusWord() {
    int result;
#if defined(ARCH_CPU_ARM64)
    asm volatile("mrs %x[result], FPCR" : [result] "=r"(result));
#else
    asm volatile("vmrs %[result], FPSCR" : [result] "=r"(result));
#endif
    return result;
  }

  inline void SetStatusWord(int a) {
#if defined(ARCH_CPU_ARM64)
    asm volatile("msr FPCR, %x[src]" : : [src] "r"(a));
#else
    asm volatile("vmsr FPSCR, %[src]" : : [src] "r"(a));
#endif
  }

#endif
};

#else
// FIXME: add implementations for other architectures and compilers
class DenormalDisabler {
  STACK_ALLOCATED();

 public:
  DenormalDisabler() {}

  // Assume the worst case that other architectures and compilers
  // need to flush denormals to zero manually.
  static inline float FlushDenormalFloatToZero(float f) {
    return (fabs(f) < FLT_MIN) ? 0.0f : f;
  }
};

#endif

}  // namespace blink

#undef HAVE_DENORMAL
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_DENORMAL_DISABLER_H_
