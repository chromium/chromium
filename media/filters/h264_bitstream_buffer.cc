// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/h264_bitstream_buffer.h"

#include "base/bits.h"
#include "base/sys_byteorder.h"

namespace media {

H264BitstreamBuffer::H264BitstreamBuffer() : data_(nullptr) {
  Reset();
}

H264BitstreamBuffer::~H264BitstreamBuffer() {
  free(data_);
  data_ = nullptr;
}

void H264BitstreamBuffer::Reset() {
  free(data_);
  data_ = nullptr;

  capacity_ = 0;
  pos_ = 0;
  bits_in_buffer_ = 0;
  reg_ = 0;

  Grow();

  bits_left_in_reg_ = kRegBitSize;
}

void H264BitstreamBuffer::Grow() {
  data_ = static_cast<uint8_t*>(realloc(data_, capacity_ + kGrowBytes));
  CHECK(data_) << "Failed growing the buffer";
  capacity_ += kGrowBytes;
}

void H264BitstreamBuffer::FlushReg() {
  // Flush all bytes that have at least one bit cached, but not more
  // (on Flush(), reg_ may not be full).
  size_t bits_in_reg = kRegBitSize - bits_left_in_reg_;
  if (bits_in_reg == 0)
    return;

  size_t bytes_in_reg = base::bits::AlignUp(bits_in_reg, size_t{8}) / 8;
  reg_ <<= (kRegBitSize - bits_in_reg);

  // Convert to MSB and append as such to the stream.
  reg_ = base::HostToNet64(reg_);

  // Make sure we have enough space. Grow() will CHECK() on allocation failure.
  if (pos_ + bytes_in_reg > capacity_)
    Grow();

  memcpy(data_ + pos_, &reg_, bytes_in_reg);
  bits_in_buffer_ = pos_ * 8 + bits_in_reg;
  pos_ += bytes_in_reg;

  reg_ = 0;
  bits_left_in_reg_ = kRegBitSize;
}

void H264BitstreamBuffer::AppendU64(size_t num_bits, uint64_t val) {
  CHECK_LE(num_bits, kRegBitSize);

  while (num_bits > 0) {
    if (bits_left_in_reg_ == 0)
      FlushReg();

    uint64_t bits_to_write =
        num_bits > bits_left_in_reg_ ? bits_left_in_reg_ : num_bits;
    uint64_t val_to_write = (val >> (num_bits - bits_to_write));
    if (bits_to_write < 64)
      val_to_write &= ((1ull << bits_to_write) - 1);
    reg_ <<= bits_to_write;
    reg_ |= val_to_write;
    num_bits -= bits_to_write;
    bits_left_in_reg_ -= bits_to_write;
  }
}

void H264BitstreamBuffer::AppendBool(bool val) {
  if (bits_left_in_reg_ == 0)
    FlushReg();

  reg_ <<= 1;
  reg_ |= (static_cast<uint64_t>(val) & 1);
  --bits_left_in_reg_;
}

void H264BitstreamBuffer::AppendSE(int val) {
  if (val > 0)
    AppendUE(val * 2 - 1);
  else
    AppendUE(-val * 2);
}

void H264BitstreamBuffer::AppendUE(unsigned int val) {
  size_t num_zeros = 0;
  unsigned int v = val + 1;

  while (v > 1) {
    v >>= 1;
    ++num_zeros;
  }

  AppendBits(num_zeros, 0);
  AppendBits(num_zeros + 1, val + 1);
}

#define DCHECK_FINISHED()                                                      \
  DCHECK_EQ(bits_left_in_reg_, kRegBitSize) << "Pending bits not yet written " \
                                               "to the buffer, call "          \
                                               "FinishNALU() first."

void H264BitstreamBuffer::BeginNALU(H264NALU::Type nalu_type, int nal_ref_idc) {
  DCHECK_FINISHED();

  DCHECK_LE(nalu_type, H264NALU::kEOStream);
  DCHECK_GE(nal_ref_idc, 0);
  DCHECK_LE(nal_ref_idc, 3);

  AppendBits(32, 0x00000001);
  AppendBits(1, 0);  // forbidden_zero_bit
  AppendBits(2, nal_ref_idc);
  AppendBits(5, nalu_type);
}

void H264BitstreamBuffer::FinishNALU() {
  // RBSP stop one bit.
  AppendBits(1, 1);

  // Byte-alignment zero bits.
  AppendBits(bits_left_in_reg_ % 8, 0);

  Flush();
}

void H264BitstreamBuffer::Flush() {
  if (bits_left_in_reg_ != kRegBitSize)
    FlushReg();
}

size_t H264BitstreamBuffer::BitsInBuffer() const {
  return bits_in_buffer_;
}

size_t H264BitstreamBuffer::BytesInBuffer() const {
  DCHECK_FINISHED();
  return pos_;
}

const uint8_t* H264BitstreamBuffer::data() const {
  DCHECK(data_);
  DCHECK_FINISHED();

  return data_;
}

}  // namespace media
