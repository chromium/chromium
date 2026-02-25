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

#include <algorithm>
#include <limits>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

template <typename T>
class AudioArray final {
  USING_FAST_MALLOC(AudioArray);

  struct BufferDeleter {
    void operator()(T* ptr) const { Partitions::BufferAlignedFree(ptr); }
  };
  // Use a local alias for HeapArray with a custom deleter that uses the
  // PartitionAlloc 'Buffer' partition. This is named PartitionHeapArray to
  // avoid confusion with base::AlignedHeapArray, which uses a different
  // deleter.
  using PartitionHeapArray = base::HeapArray<T, BufferDeleter>;

 public:
  AudioArray() = default;
  explicit AudioArray(size_t n) { CHECK(TryAllocate(n)); }
  AudioArray(const AudioArray&) = delete;
  AudioArray& operator=(const AudioArray&) = delete;

  ~AudioArray() = default;

  // It's OK to call Allocate() multiple times, but data will *not* be copied
  // from an initial allocation if re-allocated. Allocations are
  // zero-initialized.
  void Allocate(size_t n) { CHECK(TryAllocate(n)); }

  bool TryAllocate(size_t n) {
    // Although n is a size_t, its true limit is max unsigned because we use
    // unsigned in zeroRange() and copyToRange(). Also check for integer
    // overflow.
    if (n > std::numeric_limits<unsigned>::max() / sizeof(T)) {
      return false;
    }

    if (n == 0) {
      allocation_ = {};
      return true;
    }

    uint32_t initial_size = static_cast<uint32_t>(sizeof(T) * n);

    // Minimum alignment requirements for arrays so that we can use SIMD.
#if defined(ARCH_CPU_X86_FAMILY)
    const unsigned kAlignment = 32;
#else
    const unsigned kAlignment = 16;
#endif

    allocation_ = {};

    T* ptr = static_cast<T*>(Partitions::BufferTryAlignedZeroedMalloc(
        initial_size, kAlignment, WTF_HEAP_PROFILER_TYPE_NAME(AudioArray<T>)));
    if (!ptr) {
      return false;
    }

    // SAFETY: `ptr` is allocated with `initial_size` which is `n * sizeof(T)`,
    // so it has space for exactly `n` elements of type `T`.
    allocation_ =
        UNSAFE_BUFFERS(PartitionHeapArray::FromOwningPointer(ptr, n));
    return true;
  }

  T* Data() { return allocation_.data(); }
  const T* Data() const { return allocation_.data(); }
  uint32_t size() const { return static_cast<uint32_t>(allocation_.size()); }
  base::span<T> as_span() { return allocation_.as_span(); }
  base::span<const T> as_span() const { return allocation_.as_span(); }

  T& at(size_t i) {
    // Note that although it is a size_t, `size_` is now guaranteed to be
    // no greater than max unsigned. This guarantee is enforced in Allocate().
    SECURITY_DCHECK(i < size());
    return as_span()[i];
  }
  const T& at(size_t i) const {
    // Note that although it is a size_t, `size_` is now guaranteed to be
    // no greater than max unsigned. This guarantee is enforced in Allocate().
    SECURITY_DCHECK(i < size());
    return as_span()[i];
  }

  T& operator[](size_t i) { return at(i); }
  const T& operator[](size_t i) const { return at(i); }

  void Zero() {
    // This multiplication is made safe by the check in Allocate().
    std::ranges::fill(as_span(), 0);
  }

  void ZeroRange(unsigned start, unsigned end) {
    bool is_safe = (start <= end) && (end <= size());
    DCHECK(is_safe);
    if (!is_safe) {
      return;
    }

    // This expression cannot overflow because end - start cannot be
    // greater than `size_`, which is safe due to the check in Allocate().
    std::ranges::fill(as_span().subspan(start, end - start), 0);
  }

  void CopyToRange(const T* source_data, unsigned start, unsigned end) {
    bool is_safe = (start <= end) && (end <= size());
    DCHECK(is_safe);
    if (!is_safe) {
      return;
    }

    // This expression cannot overflow because end - start cannot be
    // greater than `size_`, which is safe due to the check in Allocate().
    as_span()
        .subspan(start, end - start)
        .copy_from(
            // SAFETY: `is_safe` ensures `source_data` and `end - start` are
            // safe.
            UNSAFE_BUFFERS(base::span(source_data, end - start)));
  }

 private:
  PartitionHeapArray allocation_;
};

typedef AudioArray<float> AudioFloatArray;
typedef AudioArray<double> AudioDoubleArray;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_ARRAY_H_
