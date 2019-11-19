/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BLOOM_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BLOOM_FILTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace WTF {

// Counting bloom filter with k=2 and 8 bit counters. Uses 2^keyBits bytes of
// memory.  False positive rate is approximately (1-e^(-2n/m))^2, where n is
// the number of unique keys and m is the table size (==2^keyBits).
template <unsigned keyBits>
class BloomFilter {
  USING_FAST_MALLOC(BloomFilter);

 public:
  static_assert(keyBits <= 16, "bloom filter key size check");

  static const size_t kTableSize = 1 << keyBits;
  static const unsigned kKeyMask = (1 << keyBits) - 1;
  static uint8_t MaximumCount() { return std::numeric_limits<uint8_t>::max(); }

  BloomFilter() { Clear(); }

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

  void Add(const AtomicString& string) { Add(string.Impl()->ExistingHash()); }
  void Add(const String& string) { Add(string.Impl()->GetHash()); }
  void Remove(const AtomicString& string) {
    Remove(string.Impl()->ExistingHash());
  }
  void Remove(const String& string) { Remove(string.Impl()->GetHash()); }

  bool MayContain(const AtomicString& string) const {
    return MayContain(string.Impl()->ExistingHash());
  }
  bool MayContain(const String& string) const {
    return MayContain(string.Impl()->GetHash());
  }

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
inline void BloomFilter<keyBits>::Add(unsigned hash) {
  uint8_t& first = FirstSlot(hash);
  uint8_t& second = SecondSlot(hash);
  if (LIKELY(first < MaximumCount()))
    ++first;
  if (LIKELY(second < MaximumCount()))
    ++second;
}

template <unsigned keyBits>
inline void BloomFilter<keyBits>::Remove(unsigned hash) {
  uint8_t& first = FirstSlot(hash);
  uint8_t& second = SecondSlot(hash);
  DCHECK(first);
  DCHECK(second);
  // In case of an overflow, the slot sticks in the table until clear().
  if (LIKELY(first < MaximumCount()))
    --first;
  if (LIKELY(second < MaximumCount()))
    --second;
}

template <unsigned keyBits>
inline void BloomFilter<keyBits>::Clear() {
  memset(table_, 0, kTableSize);
}

#if DCHECK_IS_ON()
template <unsigned keyBits>
bool BloomFilter<keyBits>::LikelyEmpty() const {
  for (size_t n = 0; n < kTableSize; ++n) {
    if (table_[n] && table_[n] != MaximumCount())
      return false;
  }
  return true;
}

template <unsigned keyBits>
bool BloomFilter<keyBits>::IsClear() const {
  for (size_t n = 0; n < kTableSize; ++n) {
    if (table_[n])
      return false;
  }
  return true;
}
#endif

}  // namespace WTF

using WTF::BloomFilter;

#endif
