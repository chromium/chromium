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

#include <string.h>

#include "base/macros.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

template <typename T>
class AudioArray {
  USING_FAST_MALLOC(AudioArray);

 public:
  AudioArray() : allocation_(nullptr), aligned_data_(nullptr), size_(0) {}
  explicit AudioArray(size_t n)
      : allocation_(nullptr), aligned_data_(nullptr), size_(0) {
    Allocate(n);
  }

  ~AudioArray() { WTF::Partitions::FastFree(allocation_); }

  // It's OK to call Allocate() multiple times, but data will *not* be copied
  // from an initial allocation if re-allocated. Allocations are
  // zero-initialized.
  void Allocate(size_t n) {
    // Although n is a size_t, its true limit is max unsigned because we use
    // unsigned in zeroRange() and copyToRange(). Also check for integer
    // overflow.
    CHECK_LE(n, std::numeric_limits<unsigned>::max() / sizeof(T));
    uint32_t initial_size = static_cast<uint32_t>(sizeof(T) * n);

#if defined(ARCH_CPU_X86_FAMILY) || defined(WTF_USE_WEBAUDIO_FFMPEG)
    const unsigned kAlignment = 32;
#else
    const unsigned kAlignment = 16;
#endif

    if (allocation_)
      WTF::Partitions::FastFree(allocation_);

    bool is_allocation_good = false;

    while (!is_allocation_good) {
      // Initially we try to allocate the exact size, but if it's not aligned
      // then we'll have to reallocate and from then on allocate extra.
      static unsigned extra_allocation_bytes = 0;

      unsigned total =
          base::CheckAdd(initial_size, extra_allocation_bytes).ValueOrDie();
      T* allocation = static_cast<T*>(WTF::Partitions::FastZeroedMalloc(
          total, WTF_HEAP_PROFILER_TYPE_NAME(AudioArray<T>)));
      CHECK(allocation);

      T* aligned_data = AlignedAddress(allocation, kAlignment);

      if (aligned_data == allocation || extra_allocation_bytes == kAlignment) {
        allocation_ = allocation;
        aligned_data_ = aligned_data;
        size_ = static_cast<uint32_t>(n);
        is_allocation_good = true;
      } else {
        // always allocate extra after the first alignment failure.
        extra_allocation_bytes = kAlignment;
        WTF::Partitions::FastFree(allocation);
      }
    }
  }

  T* Data() { return aligned_data_; }
  const T* Data() const { return aligned_data_; }
  uint32_t size() const { return size_; }

  T& at(size_t i) {
    // Note that although it is a size_t, m_size is now guaranteed to be
    // no greater than max unsigned. This guarantee is enforced in Allocate().
    SECURITY_DCHECK(i < size());
    return Data()[i];
  }

  T& operator[](size_t i) { return at(i); }

  void Zero() {
    // This multiplication is made safe by the check in Allocate().
    memset(this->Data(), 0, sizeof(T) * this->size());
  }

  void ZeroRange(unsigned start, unsigned end) {
    bool is_safe = (start <= end) && (end <= this->size());
    DCHECK(is_safe);
    if (!is_safe)
      return;

    // This expression cannot overflow because end - start cannot be
    // greater than m_size, which is safe due to the check in Allocate().
    memset(this->Data() + start, 0, sizeof(T) * (end - start));
  }

  void CopyToRange(const T* source_data, unsigned start, unsigned end) {
    bool is_safe = (start <= end) && (end <= this->size());
    DCHECK(is_safe);
    if (!is_safe)
      return;

    // This expression cannot overflow because end - start cannot be
    // greater than m_size, which is safe due to the check in Allocate().
    memcpy(this->Data() + start, source_data, sizeof(T) * (end - start));
  }

 private:
  static T* AlignedAddress(T* address, intptr_t alignment) {
    intptr_t value = reinterpret_cast<intptr_t>(address);
    return reinterpret_cast<T*>((value + alignment - 1) & ~(alignment - 1));
  }

  T* allocation_;
  T* aligned_data_;
  uint32_t size_;

  DISALLOW_COPY_AND_ASSIGN(AudioArray);
};

typedef AudioArray<float> AudioFloatArray;
typedef AudioArray<double> AudioDoubleArray;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_ARRAY_H_
