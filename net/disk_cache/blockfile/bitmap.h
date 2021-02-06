// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BLOCKFILE_BITMAP_H_
#define NET_DISK_CACHE_BLOCKFILE_BITMAP_H_

#include <stdint.h>
#include <string.h>

#include "base/macros.h"
#include "net/base/net_export.h"

namespace disk_cache {

// This class provides support for simple maps of bits.
class NET_EXPORT_PRIVATE Bitmap {
 public:
  Bitmap() : map_(nullptr), num_bits_(0), array_size_(0), alloc_(false) {}

  // This constructor will allocate on a uint32_t boundary. If |clear_bits| is
  // false, the bitmap bits will not be initialized.
  Bitmap(int num_bits, bool clear_bits);

  // Constructs a Bitmap with the actual storage provided by the caller. |map|
  // has to be valid until this object destruction. |num_bits| is the number of
  // bits in the bitmap, and |num_words| is the size of |map| in 32-bit words.
  Bitmap(uint32_t* map, int num_bits, int num_words);

  ~Bitmap();

  // Resizes the bitmap.
  // If |num_bits| < Size(), the extra bits will be discarded.
  // If |num_bits| > Size(), the extra bits will be filled with zeros if
  // |clear_bits| is true.
  // This object cannot be using memory provided during construction.
  void Resize(int num_bits, bool clear_bits);

  // Returns the number of bits in the bitmap.
  int Size() const { return num_bits_; }

  // Returns the number of 32-bit words in the bitmap.
  int ArraySize() const { return array_size_; }

  // Sets all the bits to true or false.
  void SetAll(bool value) {
    memset(map_, (value ? 0xFF : 0x00), array_size_ * sizeof(*map_));
  }

  // Clears all bits in the bitmap
  void Clear() { SetAll(false); }

  // Sets the value, gets the value or toggles the value of a given bit.
  void Set(int index, bool value);
  bool Get(int index) const;
  void Toggle(int index);

  // Directly sets an element of the internal map. Requires |array_index| <
  // ArraySize();
  void SetMapElement(int array_index, uint32_t value);

  // Gets an entry of the internal map. Requires array_index <
  // ArraySize()
  uint32_t GetMapElement(int array_index) const;

  // Directly sets the whole internal map. |size| is the number of 32-bit words
  // to set from |map|. If  |size| > array_size(), it ignores the end of |map|.
  void SetMap(const uint32_t* map, int size);

  // Gets a pointer to the internal map.
  const uint32_t* GetMap() const { return map_; }

  // Sets a range of bits to |value|.
  void SetRange(int begin, int end, bool value);

  // Returns true if any bit between begin inclusive and end exclusive is set.
  // 0 <= |begin| <= |end| <= Size() is required.
  bool TestRange(int begin, int end, bool value) const;

  // Scans bits starting at bit *|index|, looking for a bit set to |value|. If
  // it finds that bit before reaching bit index |limit|, sets *|index| to the
  // bit index and returns true. Otherwise returns false.
  // Requires |limit| <= Size().
  //
  // Note that to use these methods in a loop you must increment the index
  // after each use, as in:
  //
  //  for (int index = 0 ; map.FindNextBit(&index, limit, value) ; ++index) {
  //    DoSomethingWith(index);
  //  }
  bool FindNextBit(int* index, int limit, bool value) const;

  // Finds the first offset >= *|index| and < |limit| that has its bit set.
  // See FindNextBit() for more info.
  bool FindNextSetBitBeforeLimit(int* index, int limit) const {
    return FindNextBit(index, limit, true);
  }

  // Finds the first offset >= *|index| that has its bit set.
  // See FindNextBit() for more info.
  bool FindNextSetBit(int *index) const {
    return FindNextSetBitBeforeLimit(index, num_bits_);
  }

  // Scans bits starting at bit *|index|, looking for a bit set to |value|. If
  // it finds that bit before reaching bit index |limit|, sets *|index| to the
  // bit index and then counts the number of consecutive bits set to |value|
  // (before reaching |limit|), and returns that count. If no bit is found
  // returns 0. Requires |limit| <= Size().
  int FindBits(int* index, int limit, bool value) const;

  // Returns number of allocated words required for a bitmap of size |num_bits|.
  static int RequiredArraySize(int num_bits) {
    // Force at least one allocated word.
    if (num_bits <= kIntBits)
      return 1;

    return (num_bits + kIntBits - 1) >> kLogIntBits;
  }

 private:
  static const int kIntBits = sizeof(uint32_t) * 8;
  static const int kLogIntBits = 5;  // 2^5 == 32 bits per word.

  // Sets |len| bits from |start| to |value|. All the bits to be set should be
  // stored in the same word, and len < kIntBits.
  void SetWordBits(int start, int len, bool value);

  uint32_t* map_;         // The bitmap.
  int num_bits_;          // The upper bound of the bitmap.
  int array_size_;        // The physical size (in uint32s) of the bitmap.
  bool alloc_;            // Whether or not we allocated the memory.

  DISALLOW_COPY_AND_ASSIGN(Bitmap);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_BITMAP_H_
