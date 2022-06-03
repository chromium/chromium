// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/bitmap.h"

#include <algorithm>

#include "base/bits.h"
#include "base/check_op.h"

namespace {
// Returns the index of the first bit set to |value| from |word|. This code
// assumes that we'll be able to find that bit.
int FindLSBNonEmpty(uint32_t word, bool value) {
  // If we are looking for 0, negate |word| and look for 1.
  if (!value)
    word = ~word;

  return base::bits::CountTrailingZeroBits(word);
}

}  // namespace

namespace disk_cache {

Bitmap::Bitmap(int num_bits, bool clear_bits)
    : num_bits_(num_bits),
      array_size_(RequiredArraySize(num_bits)),
      alloc_(true) {
  map_ = new uint32_t[array_size_];

  // Initialize all of the bits.
  if (clear_bits)
    Clear();
}

Bitmap::Bitmap(uint32_t* map, int num_bits, int num_words)
    : map_(map),
      num_bits_(num_bits),
      // If size is larger than necessary, trim because array_size_ is used
      // as a bound by various methods.
      array_size_(std::min(RequiredArraySize(num_bits), num_words)),
      alloc_(false) {}

Bitmap::~Bitmap() {
  if (alloc_)
    delete [] map_;
}

void Bitmap::Resize(int num_bits, bool clear_bits) {
  DCHECK(alloc_ || !map_);
  const int old_maxsize = num_bits_;
  const int old_array_size = array_size_;
  array_size_ = RequiredArraySize(num_bits);

  if (array_size_ != old_array_size) {
    uint32_t* new_map = new uint32_t[array_size_];
    // Always clear the unused bits in the last word.
    new_map[array_size_ - 1] = 0;
    memcpy(new_map, map_,
           sizeof(*map_) * std::min(array_size_, old_array_size));
    if (alloc_)
      delete[] map_;  // No need to check for NULL.
    map_ = new_map;
    alloc_ = true;
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
  DCHECK_LT(array_index, array_size_);
  DCHECK_GE(array_index, 0);
  map_[array_index] = value;
}

uint32_t Bitmap::GetMapElement(int array_index) const {
  DCHECK_LT(array_index, array_size_);
  DCHECK_GE(array_index, 0);
  return map_[array_index];
}

void Bitmap::SetMap(const uint32_t* map, int size) {
  memcpy(map_, map, std::min(size, array_size_) * sizeof(*map_));
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
  memset(map_ + (begin / kIntBits), (value ? 0xFF : 0x00),
         ((end / kIntBits) - (begin / kIntBits)) * sizeof(*map_));
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
  if (begin >= end || end <= 0)
    return false;

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
