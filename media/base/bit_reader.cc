// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bit_reader.h"

#include "base/bits.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"

namespace media {

namespace {
constexpr size_t kBitsPerByte = 8u;
constexpr size_t kRegWidthInBits = sizeof(uint64_t) * kBitsPerByte;
}  // namespace

BitReader::BitReader(base::span<const uint8_t> data)
    : initial_size_(data.size()), data_(data) {}

BitReader::BitReader(const uint8_t* data, int size)
    : BitReader(
          // TODO(crbug.com/40284755): Remove this.
          UNSAFE_TODO(base::span(data, base::checked_cast<size_t>(size)))) {
  DCHECK(data != nullptr);
  DCHECK_GE(size, 0);
}

BitReader::~BitReader() = default;

bool BitReader::ReadFlag(bool* flag) {
  if (nbits_ == 0 && !Refill(1)) {
    return false;
  }

  *flag = (reg_ & base::bits::LeftmostBit<uint64_t>()) != 0;
  reg_ <<= 1;
  nbits_--;
  bits_read_++;
  return true;
}

bool BitReader::ReadString(size_t num_bits, std::string* str) {
  DCHECK_EQ(num_bits % kBitsPerByte, 0u);
  DCHECK(str);
  const int num_bytes = num_bits / kBitsPerByte;
  str->resize(num_bytes);
  return ReadSpan(base::as_writable_byte_span(*str));
}

bool BitReader::ReadSpan(base::span<uint8_t> out) {
  for (uint8_t& c : out) {
    if (!ReadBits(kBitsPerByte, &c)) {
      return false;
    }
  }
  return true;
}

bool BitReader::SkipBitsSmall(size_t num_bits) {
  uint64_t dummy;
  while (num_bits >= kRegWidthInBits) {
    if (!ReadBitsInternal(kRegWidthInBits, &dummy)) {
      return false;
    }
    num_bits -= kRegWidthInBits;
  }
  return ReadBitsInternal(num_bits, &dummy);
}

bool BitReader::SkipBits(size_t num_bits) {
  const size_t remaining_bits = nbits_ + nbits_next_;
  if (remaining_bits >= num_bits) {
    return SkipBitsSmall(num_bits);
  }

  // Skip first the remaining available bits.
  num_bits -= remaining_bits;
  bits_read_ += remaining_bits;
  nbits_ = 0;
  reg_ = 0;
  nbits_next_ = 0;
  reg_next_ = 0;

  // Next, skip an integer number of bytes.
  if (num_bits > 0) {
    const size_t nbytes = num_bits / kBitsPerByte;
    base::span<const uint8_t> byte_stream_window = GetBytes(nbytes);
    if (byte_stream_window.size() < nbytes) {
      // Note that some bytes were consumed.
      bits_read_ += kBitsPerByte * byte_stream_window.size();
      return false;
    }
    num_bits -= kBitsPerByte * nbytes;
    bits_read_ += kBitsPerByte * nbytes;
  }

  // Skip the remaining bits.
  return SkipBitsSmall(num_bits);
}

bool BitReader::ReadBitsInternal(size_t num_bits, uint64_t* out) {
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

bool BitReader::Refill(size_t min_nbits) {
  DCHECK_LE(min_nbits, kRegWidthInBits);

  // Transfer from the next to the current register.
  RefillCurrentRegister();
  if (min_nbits <= nbits_) {
    return true;
  }
  DCHECK_EQ(nbits_next_, 0u);
  DCHECK_EQ(reg_next_, 0u);

  // Max number of bytes to refill.
  static constexpr size_t kRegNextByteSize = sizeof(reg_next_);

  // Refill.
  auto byte_stream_window = GetBytes(kRegNextByteSize);
  if (byte_stream_window.empty()) {
    return false;
  }

  // Pad the window to 8 big-endian bytes to fill `reg_next_`. `reg_next_` is
  // read from the MSB, so the new bytes are written to the front in big-endian.
  std::array<uint8_t, kRegNextByteSize> bytes = {};
  base::span(bytes).copy_prefix_from(byte_stream_window);
  reg_next_ = base::U64FromBigEndian(bytes);
  nbits_next_ = byte_stream_window.size() * kBitsPerByte;

  // Transfer from the next to the current register.
  RefillCurrentRegister();

  return (nbits_ >= min_nbits);
}

void BitReader::RefillCurrentRegister() {
  // No refill possible if the destination register is full
  // or the source register is empty.
  if (nbits_ == kRegWidthInBits || nbits_next_ == 0) {
    return;
  }

  reg_ |= (reg_next_ >> nbits_);

  const size_t free_nbits = kRegWidthInBits - nbits_;
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

base::span<const uint8_t> BitReader::GetBytes(size_t max_nbytes) {
  return data_.take_first(std::min(max_nbytes, data_.size()));
}

}  // namespace media
