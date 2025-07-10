// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BIT_READER_H_
#define MEDIA_BASE_BIT_READER_H_

#include <stdint.h>

#include <string>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT BitReader {
 public:
  // Initialize the reader to start reading at |data|, |size| being size
  // of |data| in bytes.
  BitReader(const uint8_t* data, int size);

  BitReader(const BitReader&) = delete;
  BitReader& operator=(const BitReader&) = delete;

  virtual ~BitReader();

  // Read |num_bits| next bits from stream and return in |*out|, first bit
  // from the stream starting at |num_bits| position in |*out|,
  // bits of |*out| whose position is strictly greater than |num_bits|
  // are all set to zero.
  // Notes:
  // - |num_bits| cannot be larger than the bits the type can hold.
  // - From the above description, passing a signed type in |T| does not
  //   mean the first bit read from the stream gives the sign of the value.
  // Return false if the given number of bits cannot be read (not enough
  // bits in the stream), true otherwise. When return false, the stream will
  // enter a state where further ReadBits/SkipBits operations will always
  // return false unless |num_bits| is 0. The type |T| has to be a primitive
  // integer type.
  template<typename T> bool ReadBits(int num_bits, T* out) {
    DCHECK_LE(num_bits, static_cast<int>(sizeof(T) * 8));
    uint64_t temp = 0;
    bool ret = ReadBitsInternal(num_bits, &temp);
    if (ret) {
      *out = static_cast<T>(temp);
    }
    return ret;
  }

  // Read one bit from the stream and return it as a boolean in |*out|.
  // Remark: we do not use the template version for reading a bool
  // since it generates some optimization warnings during compilation
  // on Windows platforms.
  bool ReadBits(int num_bits, bool* out) {
    DCHECK_EQ(num_bits, 1);
    return ReadFlag(out);
  }

  // Read one bit from the stream and return it as a boolean in |*flag|.
  bool ReadFlag(bool* flag);

  // Read |num_bits| of binary data into |str|. |num_bits| must be a positive
  // multiple of 8. This is not efficient for extracting large strings.
  // If false is returned, |str| may not be valid.
  bool ReadString(int num_bits, std::string* str);

  // Retrieve some bits without actually consuming them.
  // Bits returned in |*out| are shifted so the most significant bit contains
  // the next bit that can be read from the stream.
  // Return the number of bits actually written in |out|.
  // Note: |num_bits| is just a suggestion of how many bits the caller
  // wish to get in |*out| and must be less than 64:
  // - The number of bits returned can be more than |num_bits|.
  // - However, it will be strictly less than |num_bits|
  //   if and only if there are not enough bits left in the stream.
  int PeekBitsMsbAligned(int num_bits, uint64_t* out);

  // Skip |num_bits| next bits from stream. Return false if the given number of
  // bits cannot be skipped (not enough bits in the stream), true otherwise.
  // When return false, the stream will enter a state where further
  // ReadBits/ReadFlag/SkipBits operations
  // will always return false unless |num_bits| is 0.
  bool SkipBits(int num_bits);

  int bits_available() const {
    return initial_size_ * 8 - bits_read();
  }

  // Returns the number of bits read so far.
  int bits_read() const;

 private:
  // This function can skip any number of bits but is more efficient
  // for small numbers. Return false if the given number of bits cannot be
  // skipped (not enough bits in the stream), true otherwise.
  bool SkipBitsSmall(int num_bits);

  // Help function used by ReadBits to avoid inlining the bit reading logic.
  bool ReadBitsInternal(int num_bits, uint64_t* out);

  // Refill bit registers to have at least |min_nbits| bits available.
  // Return true if the mininimum bit count condition is met after the refill.
  bool Refill(int min_nbits);

  // Refill the current bit register from the next bit register.
  void RefillCurrentRegister();

  int GetBytes(int max_n, const uint8_t** out);

  // Total number of bytes that was initially passed to BitReader.
  const int initial_size_;

  // Pointer to the next unread byte in the stream.
  raw_ptr<const uint8_t, AllowPtrArithmetic | DanglingUntriaged> data_;

  // Bytes left in the stream.
  int bytes_left_;

  // Number of bits read so far.
  int bits_read_ = 0;

  // Number of bits in |reg_| that have not been consumed yet.
  // Note: bits are consumed from MSB to LSB.
  int nbits_ = 0;
  uint64_t reg_ = 0;

  // Number of bits in |reg_next_| that have not been consumed yet.
  // Note: bits are consumed from MSB to LSB.
  int nbits_next_ = 0;
  uint64_t reg_next_ = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_BIT_READER_H_
