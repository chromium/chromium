// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_BITSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_BITSET_H_

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include "third_party/blink/renderer/core/css/css_property_names.h"

namespace blink {

// A bitset designed for CSSPropertyIDs.
//
// It's different from std::bitset, in that it provides optimized traversal
// for situations where only a few bits are set (which is the common case for
// e.g. CSS declarations which apply to an element).
//
// The bitset can store a configurable amount of bits for testing purposes,
// (though not more than numCSSProperties).
template <size_t kBits>
class CORE_EXPORT CSSBitsetBase {
 public:
  static_assert(
      kBits <= kNumCSSProperties,
      "Bit count must not exceed numCSSProperties, as each bit position must "
      "be representable as a CSSPropertyID");

  static const size_t kChunks = (kBits + 63) / 64;

  CSSBitsetBase() : chunks_() {}
  CSSBitsetBase(const CSSBitsetBase<kBits>& o) { *this = o; }
  CSSBitsetBase(std::initializer_list<CSSPropertyID> list) : chunks_() {
    for (CSSPropertyID id : list)
      Set(id);
  }

  void operator=(const CSSBitsetBase& o) {
    std::memcpy(chunks_, o.chunks_, sizeof(chunks_));
  }

  bool operator==(const CSSBitsetBase& o) const {
    return std::memcmp(chunks_, o.chunks_, sizeof(chunks_)) == 0;
  }
  bool operator!=(const CSSBitsetBase& o) const { return !(*this == o); }

  inline void Set(CSSPropertyID id) {
    size_t bit = static_cast<size_t>(id);
    DCHECK_LT(bit, kBits);
    chunks_[bit / 64] |= (1ull << (bit % 64));
  }

  inline void Or(CSSPropertyID id, bool v) {
    size_t bit = static_cast<size_t>(id);
    DCHECK_LT(bit, kBits);
    chunks_[bit / 64] |= (static_cast<uint64_t>(v) << (bit % 64));
  }

  inline bool Has(CSSPropertyID id) const {
    size_t bit = static_cast<size_t>(id);
    DCHECK_LT(bit, kBits);
    return chunks_[bit / 64] & (1ull << (bit % 64));
  }

  inline bool HasAny() const {
    for (uint64_t chunk : chunks_) {
      if (chunk)
        return true;
    }
    return false;
  }

  inline void Reset() { std::memset(chunks_, 0, sizeof(chunks_)); }

  // Yields the CSSPropertyIDs which are set.
  class Iterator {
   public:
    Iterator(const uint64_t* chunks, size_t index)
        : chunks_(chunks), index_(index), chunk_(ChunkAt(index)) {}

    inline void operator++() {
      do {
        ++index_;
        chunk_ = chunk_ >> 1ull;
        DCHECK_LE(index_, kBits);

        if (index_ >= kBits)
          return;
        if (!chunk_) {
          // If the chunk is empty, we can fast-forward to the next chunk.
          size_t next_chunk_index = (index_ - 1) / 64 + 1;
          index_ = std::min(next_chunk_index * 64, kBits);
          chunk_ = ChunkAt(index_);
        }
      } while (!(chunk_ & 1));
    }

    inline CSSPropertyID operator*() const {
      DCHECK_LT(index_, static_cast<size_t>(kNumCSSProperties));
      return static_cast<CSSPropertyID>(index_);
    }

    inline bool operator==(const Iterator& o) const {
      return index_ == o.index_;
    }
    inline bool operator!=(const Iterator& o) const {
      return index_ != o.index_;
    }

   private:
    // For a given index, return the corresponding chunk, down-shifted
    // such that the given index is the LSB of the chunk.
    //
    // In other words, (ChunkAt(index) & 1) is a valid way of checking whether
    // the bit at 'index' is set.
    //
    // If the given index is out of bounds, we don't really have a chunk to
    // return. This function returns 1, solely to automatically fail the
    // do-while condition in operator++. (It avoids having special handling of
    // index > kBits there).
    uint64_t ChunkAt(size_t index) const {
      return index < kBits ? chunks_[index / 64] >> (index % 64) : 1ull;
    }

    const uint64_t* chunks_;
    // The current bit index this Iterator is pointing to. Note that this is
    // the "global" index, i.e. it has the range [0, kBits]. (It is not a local
    // index with range [0, 64]).
    //
    // Never exceeds kBits.
    size_t index_ = 0;
    // The iterator works by "pre-fetching" the current chunk (corresponding
    // (to the current index), and down-shifting by one for every iteration.
    // This allows the iterator to skip the remainder of the chunk when we
    // shift away the last bit.
    uint64_t chunk_ = 0;
  };

  Iterator begin() const { return Iterator(chunks_, FirstIndex()); }
  Iterator end() const { return Iterator(chunks_, kBits); }

 private:
  // Find the first index (i.e. first set bit). If no bits are set, returns
  // kBits.
  size_t FirstIndex() const {
    size_t index = 0;
    // Skip all empty chunks.
    while (index < kBits && !chunks_[index / 64])
      index += 64;
    // Within the non-empty chunk, iterate until we find the set bit.
    while (index < kBits && !(chunks_[index / 64] & (1ull << (index % 64))))
      ++index;
    return std::min(index, kBits);
  }

  uint64_t chunks_[kChunks];
};

using CSSBitset = CSSBitsetBase<kNumCSSProperties>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_BITSET_H_
