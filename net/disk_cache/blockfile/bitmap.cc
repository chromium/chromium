// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/bitmap.h"

#include <algorithm>
#include <bit>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/numerics/safe_conversions.h"

namespace {
// Returns the index of the first bit set to |value| from |word|. This code
// assumes that we'll be able to find that bit.
int FindLSBNonEmpty(uint32_t word, bool value) {
  // If we are looking for 0, negate |word| and look for 1.
  if (!value)
    word = ~word;

  return std::countr_zero(word);
}

}  // namespace

namespace disk_cache {

base::span<uint32_t> ToUint32Span(base::span<uint8_t> in) {
  CHECK_EQ(in.size() % 4, 0u);
  CHECK_EQ(reinterpret_cast<uintptr_t>(in.data()) % alignof(uint32_t), 0u);

  // SAFETY: Above check ensures that `in` is aligned for 32-bit access;
  // and below accesses the same memory.
  return UNSAFE_BUFFERS(base::span<uint32_t>(
      reinterpret_cast<uint32_t*>(in.data()), in.size() / 4));
}

Bitmap::Bitmap() = default;

Bitmap::Bitmap(int num_bits, bool clear_bits)
    : num_bits_(num_bits),
      allocated_map_(
          base::HeapArray<uint32_t>::Uninit(RequiredArraySize(num_bits))) {
  map_ = allocated_map_.as_span();

  // Initialize all of the bits.
  if (clear_bits)
    Clear();
}

Bitmap::Bitmap(base::span<uint32_t> map, size_t num_bits)
    : num_bits_(num_bits),
      // If size is larger than necessary, trim. base::span will guard against
      // out-of-bounds accesses.
      map_(map.first(std::min(RequiredArraySize(num_bits), map.size()))) {}

Bitmap::~Bitmap() = default;

void Bitmap::Resize(int num_bits, bool clear_bits) {
  const int old_maxsize = num_bits_;
  const int old_array_size = map_.size();
  const int new_array_size = RequiredArraySize(num_bits);

  if (new_array_size != old_array_size) {
    auto new_map = base::HeapArray<uint32_t>::Uninit(new_array_size);
    // Always clear the unused bits in the last word.
    new_map[new_array_size - 1] = 0;
    new_map.copy_prefix_from(map_.subspan(
        0u,
        base::checked_cast<size_t>(std::min(new_array_size, old_array_size))));
    map_ = new_map.as_span();
    allocated_map_ = std::move(new_map);
  }

  num_bits_ = num_bits;
  if (old_maxsize < num_bits_ && clear_bits) {
    SetRange(old_maxsize, num_bits_, false);
  }
}

void Bitmap::Set(int index, bool value) {
  DCHECK_LT(index, num_bits_);
  DCHECK_GE(index, 0);
  const int i = index & (kIntBits - 1);
  const int j = index / kIntBits;
  if (value)
    map_[j] |= (1 << i);
  else
    map_[j] &= ~(1 << i);
}

bool Bitmap::Get(int index) const {
  DCHECK_LT(index, num_bits_);
  DCHECK_GE(index, 0);
  const int i = index & (kIntBits-1);
  const int j = index / kIntBits;
  return ((map_[j] & (1 << i)) != 0);
}

void Bitmap::Toggle(int index) {
  DCHECK_LT(index, num_bits_);
  DCHECK_GE(index, 0);
  const int i = index & (kIntBits - 1);
  const int j = index / kIntBits;
  map_[j] ^= (1 << i);
}

void Bitmap::SetMapElement(int array_index, uint32_t value) {
  DCHECK_GE(array_index, 0);
  map_[array_index] = value;
}

uint32_t Bitmap::GetMapElement(int array_index) const {
  DCHECK_GE(array_index, 0);
  return map_[array_index];
}

void Bitmap::SetMap(base::span<const uint32_t> map) {
  map_.copy_prefix_from(map);
}

void Bitmap::SetRange(int begin, int end, bool value) {
  DCHECK_LE(begin, end);
  int start_offset = begin & (kIntBits - 1);
  if (start_offset) {
    // Set the bits in the first word.
    int len = std::min(end - begin, kIntBits - start_offset);
    SetWordBits(begin, len, value);
    begin += len;
  }

  if (begin == end)
    return;

  // Now set the bits in the last word.
  int end_offset = end & (kIntBits - 1);
  end -= end_offset;
  SetWordBits(end, end_offset, value);

  // Set all the words in the middle.
  std::ranges::fill(map_.subspan(base::checked_cast<size_t>(begin / kIntBits),
                                 base::checked_cast<size_t>(
                                     (end / kIntBits) - (begin / kIntBits))),
                    (value ? 0xFFFFFFFFu : 0x00u));
}

// Return true if any bit between begin inclusive and end exclusive
// is set.  0 <= begin <= end <= bits() is required.
bool Bitmap::TestRange(int begin, int end, bool value) const {
  DCHECK_LT(begin, num_bits_);
  DCHECK_LE(end, num_bits_);
  DCHECK_LE(begin, end);
  DCHECK_GE(begin, 0);
  DCHECK_GE(end, 0);

  // Return false immediately if the range is empty.
  if (begin == end) {
    return false;
  }

  // Calculate the indices of the words containing the first and last bits,
  // along with the positions of the bits within those words.
  int word = begin / kIntBits;
  int offset = begin & (kIntBits - 1);
  int last_word = (end - 1) / kIntBits;
  int last_offset = (end - 1) & (kIntBits - 1);

  // If we are looking for zeros, negate the data from the map.
  uint32_t this_word = map_[word];
  if (!value)
    this_word = ~this_word;

  // If the range spans multiple words, discard the extraneous bits of the
  // first word by shifting to the right, and then test the remaining bits.
  if (word < last_word) {
    if (this_word >> offset)
      return true;
    offset = 0;

    word++;
    // Test each of the "middle" words that lies completely within the range.
    while (word < last_word) {
      this_word = map_[word++];
      if (!value)
        this_word = ~this_word;
      if (this_word)
        return true;
    }
  }

  // Test the portion of the last word that lies within the range. (This logic
  // also handles the case where the entire range lies within a single word.)
  const uint32_t mask = ((2 << (last_offset - offset)) - 1) << offset;

  this_word = map_[last_word];
  if (!value)
    this_word = ~this_word;

  return (this_word & mask) != 0;
}

bool Bitmap::FindNextBit(int* index, int limit, bool value) const {
  DCHECK_LT(*index, num_bits_);
  DCHECK_LE(limit, num_bits_);
  DCHECK_LE(*index, limit);
  DCHECK_GE(*index, 0);
  DCHECK_GE(limit, 0);

  const int bit_index = *index;
  if (bit_index >= limit || limit <= 0)
    return false;

  // From now on limit != 0, since if it was we would have returned false.
  int word_index = bit_index >> kLogIntBits;
  uint32_t one_word = map_[word_index];

  // Simple optimization where we can immediately return true if the first
  // bit is set.  This helps for cases where many bits are set, and doesn't
  // hurt too much if not.
  if (Get(bit_index) == value)
    return true;

  const int first_bit_offset = bit_index & (kIntBits - 1);

  // First word is special - we need to mask off leading bits.
  uint32_t mask = 0xFFFFFFFF << first_bit_offset;
  if (value) {
    one_word &= mask;
  } else {
    one_word |= ~mask;
  }

  uint32_t empty_value = value ? 0 : 0xFFFFFFFF;

  // Loop through all but the last word.  Note that 'limit' is one
  // past the last bit we want to check, and we don't want to read
  // past the end of "words".  E.g. if num_bits_ == 32 only words[0] is
  // valid, so we want to avoid reading words[1] when limit == 32.
  const int last_word_index = (limit - 1) >> kLogIntBits;
  while (word_index < last_word_index) {
    if (one_word != empty_value) {
      *index = (word_index << kLogIntBits) + FindLSBNonEmpty(one_word, value);
      return true;
    }
    one_word = map_[++word_index];
  }

  // Last word is special - we may need to mask off trailing bits.  Note that
  // 'limit' is one past the last bit we want to check, and if limit is a
  // multiple of 32 we want to check all bits in this word.
  const int last_bit_offset = (limit - 1) & (kIntBits - 1);
  mask = 0xFFFFFFFE << last_bit_offset;
  if (value) {
    one_word &= ~mask;
  } else {
    one_word |= mask;
  }
  if (one_word != empty_value) {
    *index = (word_index << kLogIntBits) + FindLSBNonEmpty(one_word, value);
    return true;
  }
  return false;
}

int Bitmap::FindBits(int* index, int limit, bool value) const {
  DCHECK_LT(*index, num_bits_);
  DCHECK_LE(limit, num_bits_);
  DCHECK_LE(*index, limit);
  DCHECK_GE(*index, 0);
  DCHECK_GE(limit, 0);

  if (!FindNextBit(index, limit, value))
    return false;

  // Now see how many bits have the same value.
  int end = *index;
  if (!FindNextBit(&end, limit, !value))
    return limit - *index;

  return end - *index;
}

void Bitmap::SetWordBits(int start, int len, bool value) {
  DCHECK_LT(len, kIntBits);
  DCHECK_GE(len, 0);
  if (!len)
    return;

  int word = start / kIntBits;
  int offset = start % kIntBits;

  uint32_t to_add = 0xffffffff << len;
  to_add = (~to_add) << offset;
  if (value) {
    map_[word] |= to_add;
  } else {
    map_[word] &= ~to_add;
  }
}

}  // namespace disk_cache
