/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BLOOM_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BLOOM_FILTER_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace WTF {

// Bloom filter with k=2. Uses 2^keyBits/8 bytes of memory.
// False positive rate is approximately (1-e^(-2n/m))^2, where n is the number
// of unique  keys and m is the table size (==2^keyBits).
template <unsigned keyBits>
class BloomFilter {
  USING_FAST_MALLOC(BloomFilter);

 public:
  BloomFilter() { Clear(); }

  void Add(unsigned hash);

  // The filter may give false positives (claim it may contain a key it doesn't)
  // but never false negatives (claim it doesn't contain a key it does).
  bool MayContain(unsigned hash) const;

  // The filter must be cleared before reuse.
  void Clear();

  // Add every element from the other filter into this one, so that
  // if this->MayContain(hash) || other.MayContain(hash) before the call,
  // this->MayContain(hash) will be true after it.
  void Merge(const BloomFilter<keyBits>& other);

  friend bool operator==(const BloomFilter<keyBits>& a,
                         const BloomFilter<keyBits>& b) {
    return memcmp(a.bit_array_, b.bit_array_, a.kBitArrayMemorySize) == 0;
  }

  unsigned* GetRawData() { return bit_array_; }

 private:
  using BitArrayUnit = unsigned;
  static constexpr size_t kMaxKeyBits = 16;
  static constexpr size_t kTableSize = 1 << keyBits;
  static constexpr size_t kBitsPerPosition = 8 * sizeof(BitArrayUnit);
  static constexpr size_t kBitArraySize = kTableSize / kBitsPerPosition;
  static constexpr size_t kBitArrayMemorySize =
      kBitArraySize * sizeof(BitArrayUnit);
  static constexpr unsigned kKeyMask = (1 << keyBits) - 1;

  static size_t BitArrayIndex(unsigned key);
  static unsigned BitMask(unsigned key);

  bool IsBitSet(unsigned key) const;
  void SetBit(unsigned key);

  BitArrayUnit bit_array_[kBitArraySize];

  static_assert(keyBits <= kMaxKeyBits, "bloom filter key size check");

  friend class BloomFilterTest;
};

template <unsigned keyBits>
inline bool BloomFilter<keyBits>::MayContain(unsigned hash) const {
  // The top and bottom bits of the incoming hash are treated as independent
  // bloom filter hash functions. This works well as long as the filter size
  // is not much above 2^kMaxKeyBits
  return IsBitSet(hash) && IsBitSet(hash >> kMaxKeyBits);
}

template <unsigned keyBits>
inline void BloomFilter<keyBits>::Add(unsigned hash) {
  SetBit(hash);
  SetBit(hash >> kMaxKeyBits);
}

template <unsigned keyBits>
inline void BloomFilter<keyBits>::Clear() {
  memset(bit_array_, 0, kBitArrayMemorySize);
}

template <unsigned keyBits>
inline void BloomFilter<keyBits>::Merge(const BloomFilter<keyBits>& other) {
  for (size_t i = 0; i < kBitArraySize; ++i) {
    bit_array_[i] |= other.bit_array_[i];
  }
}

template <unsigned keyBits>
inline size_t BloomFilter<keyBits>::BitArrayIndex(unsigned key) {
  return (key & kKeyMask) / kBitsPerPosition;
}

template <unsigned keyBits>
inline unsigned BloomFilter<keyBits>::BitMask(unsigned key) {
  return 1 << (key % kBitsPerPosition);
}

template <unsigned keyBits>
bool BloomFilter<keyBits>::IsBitSet(unsigned key) const {
  DCHECK_LT(BitArrayIndex(key), kBitArraySize);
  return bit_array_[BitArrayIndex(key)] & BitMask(key);
}

template <unsigned keyBits>
void BloomFilter<keyBits>::SetBit(unsigned key) {
  DCHECK_LT(BitArrayIndex(key), kBitArraySize);
  bit_array_[BitArrayIndex(key)] |= BitMask(key);
}

// Counting bloom filter with k=2 and 8 bit counters. Uses 2^keyBits bytes of
// memory.  False positive rate is approximately (1-e^(-2n/m))^2, where n is
// the number of unique keys and m is the table size (==2^keyBits).
template <unsigned keyBits>
class CountingBloomFilter {
  USING_FAST_MALLOC(CountingBloomFilter);

 public:
  static_assert(keyBits <= 16, "bloom filter key size check");

  static const size_t kTableSize = 1 << keyBits;
  static const unsigned kKeyMask = (1 << keyBits) - 1;
  static uint8_t MaximumCount() { return std::numeric_limits<uint8_t>::max(); }

  CountingBloomFilter() { Clear(); }

  void Add(unsigned hash);
  void Remove(unsigned hash);

  // The filter may give false positives (claim it may contain a key it doesn't)
  // but never false negatives (claim it doesn't contain a key it does).
  bool MayContain(unsigned hash) const {
    return FirstSlot(hash) && SecondSlot(hash);
  }

  // The filter must be cleared before reuse even if all keys are removed.
  // Otherwise overflowed keys will stick around.
  void Clear();

#if DCHECK_IS_ON()
  // Slow.
  bool LikelyEmpty() const;
  bool IsClear() const;
#endif

 private:
  uint8_t& FirstSlot(unsigned hash) { return table_[hash & kKeyMask]; }
  uint8_t& SecondSlot(unsigned hash) { return table_[(hash >> 16) & kKeyMask]; }
  const uint8_t& FirstSlot(unsigned hash) const {
    return table_[hash & kKeyMask];
  }
  const uint8_t& SecondSlot(unsigned hash) const {
    return table_[(hash >> 16) & kKeyMask];
  }

  uint8_t table_[kTableSize];
};

template <unsigned keyBits>
inline void CountingBloomFilter<keyBits>::Add(unsigned hash) {
  uint8_t& first = FirstSlot(hash);
  uint8_t& second = SecondSlot(hash);
  if (first < MaximumCount()) [[likely]] {
    ++first;
  }
  if (second < MaximumCount()) [[likely]] {
    ++second;
  }
}

template <unsigned keyBits>
inline void CountingBloomFilter<keyBits>::Remove(unsigned hash) {
  uint8_t& first = FirstSlot(hash);
  uint8_t& second = SecondSlot(hash);
  DCHECK(first);
  DCHECK(second);
  // In case of an overflow, the slot sticks in the table until clear().
  if (first < MaximumCount()) [[likely]] {
    --first;
  }
  if (second < MaximumCount()) [[likely]] {
    --second;
  }
}

template <unsigned keyBits>
inline void CountingBloomFilter<keyBits>::Clear() {
  memset(table_, 0, kTableSize);
}

#if DCHECK_IS_ON()
template <unsigned keyBits>
bool CountingBloomFilter<keyBits>::LikelyEmpty() const {
  for (size_t n = 0; n < kTableSize; ++n) {
    if (table_[n] && table_[n] != MaximumCount())
      return false;
  }
  return true;
}

template <unsigned keyBits>
bool CountingBloomFilter<keyBits>::IsClear() const {
  for (size_t n = 0; n < kTableSize; ++n) {
    if (table_[n])
      return false;
  }
  return true;
}
#endif

}  // namespace WTF

using WTF::CountingBloomFilter;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BLOOM_FILTER_H_
