/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_ARRAY_H_

#include <concepts>

#include "base/compiler_specific.h"
#include "base/memory/aligned_memory.h"
#include "build/build_config.h"

namespace blink {

template <typename T>
class GSL_OWNER AudioArray {
 public:
  // Other trivially constructible and destructible types would also be valid,
  // but guard against the only types that are used for now.
  static_assert(std::same_as<T, float> || std::same_as<T, double>,
                "AudioArray must be float or double");

  using iterator = base::AlignedHeapArray<T>::iterator;
  using const_iterator = base::AlignedHeapArray<T>::const_iterator;

  AudioArray() = default;
  explicit AudioArray(size_t n) { Allocate(n); }

  AudioArray(const AudioArray&) = delete;
  AudioArray& operator=(const AudioArray&) = delete;

  ~AudioArray() = default;

  // It's OK to call Allocate() multiple times, but data will *not* be
  // copied from an initial allocation if re-allocated. Allocations are
  // zero-initialized.
  void Allocate(size_t n) {
    // Minimum alignment requirements for arrays so that we can use SIMD.
#if defined(ARCH_CPU_X86_FAMILY)
    static constexpr unsigned kAlignment = 32;
#else
    static constexpr unsigned kAlignment = 16;
#endif

    data_ = base::AlignedUninit<T>(n, kAlignment);

#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_ANDROID)
    // The android emulators crash when accessing the begin()/end() iterators
    // for `data_` for some reason. Manually set this memory here, before the
    // first time it's accessed.
    // SAFETY: `data_` owns exactly `n` elements of type T. The
    // `base::AlignedUninit` already CHECKs if `n * sizeof(T)` overflows.
    UNSAFE_BUFFERS(memset(data_.data(), 0, n * sizeof(T)));
#else
    Zero();
#endif
  }

  // TODO(crbug.com/375449662): Attempt to remove these functions, and favor
  // range-based operations. If this isn't possible, at least mark these these
  // functions as UNSAFE_BUFFER_USAGE.
  T* Data() { return data_.data(); }
  const T* Data() const { return data_.data(); }
  uint32_t size() const { return data_.size(); }

  iterator begin() { return data_.begin(); }
  const_iterator begin() const { return data_.begin(); }

  iterator end() { return data_.end(); }
  const_iterator end() const { return data_.end(); }

  T& at(size_t i) { return data_[i]; }

  T& operator[](size_t i) { return at(i); }

  void Zero() { std::ranges::fill(data_, 0); }

  void ZeroRange(unsigned start, unsigned end) {
    std::ranges::fill(data_.subspan(start, end - start), 0);
  }

 private:
  base::AlignedHeapArray<T> data_;
};

typedef AudioArray<float> AudioFloatArray;
typedef AudioArray<double> AudioDoubleArray;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_ARRAY_H_
