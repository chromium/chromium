// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/h26x_annex_b_bitstream_builder.h"

#include "base/bits.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"

namespace media {

H26xAnnexBBitstreamBuilder::H26xAnnexBBitstreamBuilder(
    bool insert_emulation_prevention_bytes)
    : insert_emulation_prevention_bytes_(insert_emulation_prevention_bytes) {
  Reset();
}

H26xAnnexBBitstreamBuilder::~H26xAnnexBBitstreamBuilder() = default;

void H26xAnnexBBitstreamBuilder::Reset() {
  data_ = base::HeapArray<uint8_t>();
  pos_ = 0;
  bits_in_buffer_ = 0;
  reg_ = 0;

  Grow();

  bits_left_in_reg_ = kRegBitSize;

  in_nalu_ = false;
}

void H26xAnnexBBitstreamBuilder::Grow() {
  auto grown = base::HeapArray<uint8_t>::Uninit(data_.size() + kGrowBytes);
  // The first `pos_` bytes in `data_` are initialized. Copy them but don't read
  // from the uninitialized stuff after it.
  grown.copy_prefix_from(data_.first(pos_));
  data_ = std::move(grown);
}

void H26xAnnexBBitstreamBuilder::FlushReg() {
  // Flush all bytes that have at least one bit cached, but not more
  // (on Flush(), reg_ may not be full).
  size_t bits_in_reg = kRegBitSize - bits_left_in_reg_;
  if (bits_in_reg == 0u) {
    return;
  }

  size_t bytes_in_reg = base::bits::AlignUp(bits_in_reg, size_t{8}) / 8u;
  reg_ <<= (kRegBitSize - bits_in_reg);

  // Convert to MSB and append as such to the stream.
  std::array<uint8_t, 8> reg_be = base::U64ToBigEndian(reg_);

  if (insert_emulation_prevention_bytes_ && in_nalu_) {
    // The EPB only works on complete bytes being flushed.
    CHECK_EQ(bits_in_reg % 8u, 0u);
    // Insert emulation prevention bytes (spec 7.3.1).
    constexpr uint8_t kEmulationByte = 0x03u;

    for (size_t i = 0; i < bytes_in_reg; ++i) {
      // This will possibly check the NALU header byte. However the
      // CHECK_NE(nalu_type, 0) makes sure that it is not 0.
      if (pos_ >= 2u && data_[pos_ - 2u] == 0 && data_[pos_ - 1u] == 0u &&
          reg_be[i] <= kEmulationByte) {
        if (pos_ + 1u > data_.size()) {
          Grow();
        }
        data_[pos_++] = kEmulationByte;
        bits_in_buffer_ += 8u;
      }
      if (pos_ + 1u > data_.size()) {
        Grow();
      }
      data_[pos_++] = reg_be[i];
      bits_in_buffer_ += 8u;
    }
  } else {
    // Make sure we have enough space.
    if (pos_ + bytes_in_reg > data_.size()) {
      Grow();
    }

    data_.subspan(pos_).copy_prefix_from(
        base::span(reg_be).first(bytes_in_reg));
    bits_in_buffer_ = pos_ * 8u + bits_in_reg;
    pos_ += bytes_in_reg;
  }

  reg_ = 0u;
  bits_left_in_reg_ = kRegBitSize;
}

void H26xAnnexBBitstreamBuilder::AppendU64(size_t num_bits, uint64_t val) {
  CHECK_LE(num_bits, kRegBitSize);

  while (num_bits > 0u) {
    if (bits_left_in_reg_ == 0u) {
      FlushReg();
    }

    uint64_t bits_to_write =
        num_bits > bits_left_in_reg_ ? bits_left_in_reg_ : num_bits;
    uint64_t val_to_write = (val >> (num_bits - bits_to_write));
    if (bits_to_write < 64u) {
      val_to_write &= ((1ull << bits_to_write) - 1);
      reg_ <<= bits_to_write;
      reg_ |= val_to_write;
    } else {
      reg_ = val_to_write;
    }
    num_bits -= bits_to_write;
    bits_left_in_reg_ -= bits_to_write;
  }
}

void H26xAnnexBBitstreamBuilder::AppendBool(bool val) {
  if (bits_left_in_reg_ == 0u) {
    FlushReg();
  }

  reg_ <<= 1;
  reg_ |= (static_cast<uint64_t>(val) & 1u);
  --bits_left_in_reg_;
}

void H26xAnnexBBitstreamBuilder::AppendSE(int val) {
  if (val > 0)
    AppendUE(val * 2 - 1);
  else
    AppendUE(-val * 2);
}

void H26xAnnexBBitstreamBuilder::AppendUE(unsigned int val) {
  size_t num_zeros = 0u;
  unsigned int v = val + 1u;

  while (v > 1) {
    v >>= 1;
    ++num_zeros;
  }

  AppendBits(num_zeros, 0);
  AppendBits(num_zeros + 1, val + 1u);
}

#define DCHECK_FINISHED()                                                      \
  DCHECK_EQ(bits_left_in_reg_, kRegBitSize) << "Pending bits not yet written " \
                                               "to the buffer, call "          \
                                               "FinishNALU() first."

void H26xAnnexBBitstreamBuilder::BeginNALU(H264NALU::Type nalu_type,
                                           int nal_ref_idc) {
  DCHECK(!in_nalu_);
  DCHECK_FINISHED();

  DCHECK_LE(nalu_type, H264NALU::kEOStream);
  DCHECK_GE(nal_ref_idc, 0);
  DCHECK_LE(nal_ref_idc, 3);

  AppendBits(32, 0x00000001);
  Flush();
  in_nalu_ = true;
  AppendBits(1, 0);  // forbidden_zero_bit
  AppendBits(2, nal_ref_idc);
  CHECK_NE(nalu_type, 0);
  AppendBits(5, nalu_type);
}

void H26xAnnexBBitstreamBuilder::BeginNALU(H265NALU::Type nalu_type) {
  DCHECK(!in_nalu_);
  DCHECK_FINISHED();

  DCHECK_LE(nalu_type, H265NALU::Type::EOS_NUT);

  AppendBits(32, 0x00000001);
  Flush();
  in_nalu_ = true;
  AppendBits(1, 0);          // forbidden_zero_bit
  AppendBits(6, nalu_type);  // nal_unit_type
  AppendBits(6, 0);          // nuh_layer_id
  AppendBits(3, 1);          // nuh_temporal_id_plus_1
}

void H26xAnnexBBitstreamBuilder::FinishNALU() {
  // RBSP stop one bit.
  AppendBits(1, 1);

  // Byte-alignment zero bits.
  AppendBits(bits_left_in_reg_ % 8, 0);

  Flush();
  in_nalu_ = false;
}

void H26xAnnexBBitstreamBuilder::Flush() {
  if (bits_left_in_reg_ != kRegBitSize)
    FlushReg();
}

size_t H26xAnnexBBitstreamBuilder::BitsInBuffer() const {
  return bits_in_buffer_;
}

size_t H26xAnnexBBitstreamBuilder::BytesInBuffer() const {
  DCHECK_FINISHED();
  return pos_;
}

const uint8_t* H26xAnnexBBitstreamBuilder::data() const {
  DCHECK(!data_.empty());
  DCHECK_FINISHED();

  return data_.data();
}

}  // namespace media
