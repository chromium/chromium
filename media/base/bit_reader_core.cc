// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bit_reader_core.h"

#include <stdint.h>
#include <cstring>

#include "base/sys_byteorder.h"

namespace {
const int kRegWidthInBits = sizeof(uint64_t) * 8;
}

namespace media {

BitReaderCore::ByteStreamProvider::ByteStreamProvider() = default;

BitReaderCore::ByteStreamProvider::~ByteStreamProvider() = default;

BitReaderCore::BitReaderCore(ByteStreamProvider* byte_stream_provider)
    : byte_stream_provider_(byte_stream_provider),
      bits_read_(0),
      nbits_(0),
      reg_(0),
      nbits_next_(0),
      reg_next_(0) {
}

BitReaderCore::~BitReaderCore() = default;

bool BitReaderCore::ReadFlag(bool* flag) {
  if (nbits_ == 0 && !Refill(1))
    return false;

  *flag = (reg_ & (UINT64_C(1) << (kRegWidthInBits - 1))) != 0;
  reg_ <<= 1;
  nbits_--;
  bits_read_++;
  return true;
}

int BitReaderCore::PeekBitsMsbAligned(int num_bits, uint64_t* out) {
  // Try to have at least |num_bits| in the bit register.
  if (nbits_ < num_bits)
    Refill(num_bits);

  *out = reg_;
  return nbits_;
}

bool BitReaderCore::SkipBitsSmall(int num_bits) {
  DCHECK_GE(num_bits, 0);
  uint64_t dummy;
  while (num_bits >= kRegWidthInBits) {
    if (!ReadBitsInternal(kRegWidthInBits, &dummy))
      return false;
    num_bits -= kRegWidthInBits;
  }
  return ReadBitsInternal(num_bits, &dummy);
}

bool BitReaderCore::SkipBits(int num_bits) {
  DCHECK_GE(num_bits, 0);

  const int remaining_bits = nbits_ + nbits_next_;
  if (remaining_bits >= num_bits)
    return SkipBitsSmall(num_bits);

  // Skip first the remaining available bits.
  num_bits -= remaining_bits;
  bits_read_ += remaining_bits;
  nbits_ = 0;
  reg_ = 0;
  nbits_next_ = 0;
  reg_next_ = 0;

  // Next, skip an integer number of bytes.
  const int nbytes = num_bits / 8;
  if (nbytes > 0) {
    const uint8_t* byte_stream_window;
    const int window_size =
        byte_stream_provider_->GetBytes(nbytes, &byte_stream_window);
    DCHECK_GE(window_size, 0);
    DCHECK_LE(window_size, nbytes);
    if (window_size < nbytes) {
      // Note that some bytes were consumed.
      bits_read_ += 8 * window_size;
      return false;
    }
    num_bits -= 8 * nbytes;
    bits_read_ += 8 * nbytes;
  }

  // Skip the remaining bits.
  return SkipBitsSmall(num_bits);
}

int BitReaderCore::bits_read() const {
  return bits_read_;
}

bool BitReaderCore::ReadBitsInternal(int num_bits, uint64_t* out) {
  DCHECK_GE(num_bits, 0);

  if (num_bits == 0) {
    *out = 0;
    return true;
  }

  if (num_bits > nbits_ && !Refill(num_bits)) {
    // Any subsequent ReadBits should fail:
    // empty the current bit register for that purpose.
    nbits_ = 0;
    reg_ = 0;
    *out = 0;
    return false;
  }

  bits_read_ += num_bits;

  if (num_bits == kRegWidthInBits) {
    // Special case needed since for example for a 64 bit integer "a"
    // "a << 64" is not defined by the C/C++ standard.
    *out = reg_;
    reg_ = 0;
    nbits_ = 0;
    return true;
  }

  *out = reg_ >> (kRegWidthInBits - num_bits);
  reg_ <<= num_bits;
  nbits_ -= num_bits;
  return true;
}

bool BitReaderCore::Refill(int min_nbits) {
  DCHECK_LE(min_nbits, kRegWidthInBits);

  // Transfer from the next to the current register.
  RefillCurrentRegister();
  if (min_nbits <= nbits_)
    return true;
  DCHECK_EQ(nbits_next_, 0);
  DCHECK_EQ(reg_next_, 0u);

  // Max number of bytes to refill.
  int max_nbytes = sizeof(reg_next_);

  // Refill.
  const uint8_t* byte_stream_window;
  int window_size =
      byte_stream_provider_->GetBytes(max_nbytes, &byte_stream_window);
  DCHECK_GE(window_size, 0);
  DCHECK_LE(window_size, max_nbytes);
  if (window_size == 0)
    return false;

  reg_next_ = 0;
  memcpy(&reg_next_, byte_stream_window, window_size);
  reg_next_ = base::NetToHost64(reg_next_);
  nbits_next_ = window_size * 8;

  // Transfer from the next to the current register.
  RefillCurrentRegister();

  return (nbits_ >= min_nbits);
}

void BitReaderCore::RefillCurrentRegister() {
  // No refill possible if the destination register is full
  // or the source register is empty.
  if (nbits_ == kRegWidthInBits || nbits_next_ == 0)
    return;

  reg_ |= (reg_next_ >> nbits_);

  int free_nbits = kRegWidthInBits - nbits_;
  if (free_nbits >= nbits_next_) {
    nbits_ += nbits_next_;
    reg_next_ = 0;
    nbits_next_ = 0;
    return;
  }

  nbits_ += free_nbits;
  reg_next_ <<= free_nbits;
  nbits_next_ -= free_nbits;
}

}  // namespace media
