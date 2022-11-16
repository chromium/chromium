// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BIT_READER_CORE_H_
#define MEDIA_BASE_BIT_READER_CORE_H_

#include <stdint.h>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT BitReaderCore {
 public:
  class ByteStreamProvider {
   public:
    ByteStreamProvider();
    virtual ~ByteStreamProvider();

    // Consume at most the following |max_n| bytes of the stream
    // and return the number n of bytes actually consumed.
    // Set |*array| to point to a memory buffer containing those n bytes.
    // Note: |*array| must be valid until the next call to GetBytes
    // but there is no guarantee it is valid after.
    virtual int GetBytes(int max_n, const uint8_t** array) = 0;
  };

  // Lifetime of |byte_stream_provider| must be longer than BitReaderCore.
  explicit BitReaderCore(ByteStreamProvider* byte_stream_provider);

  BitReaderCore(const BitReaderCore&) = delete;
  BitReaderCore& operator=(const BitReaderCore&) = delete;

  ~BitReaderCore();

  // Read one bit from the stream and return it as a boolean in |*out|.
  // Remark: we do not use the template version for reading a bool
  // since it generates some optimization warnings during compilation
  // on Windows platforms.
  bool ReadBits(int num_bits, bool* out) {
    DCHECK_EQ(num_bits, 1);
    return ReadFlag(out);
  }

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
    if (ret)
      *out = static_cast<T>(temp);
    return ret;
  }

  // Read one bit from the stream and return it as a boolean in |*flag|.
  bool ReadFlag(bool* flag);

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

  const raw_ptr<ByteStreamProvider, DanglingUntriaged> byte_stream_provider_;

  // Number of bits read so far.
  int bits_read_;

  // Number of bits in |reg_| that have not been consumed yet.
  // Note: bits are consumed from MSB to LSB.
  int nbits_;
  uint64_t reg_;

  // Number of bits in |reg_next_| that have not been consumed yet.
  // Note: bits are consumed from MSB to LSB.
  int nbits_next_;
  uint64_t reg_next_;
};

}  // namespace media

#endif  // MEDIA_BASE_BIT_READER_CORE_H_
