// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/av1_builder.h"

#include "base/check_op.h"

namespace media {

AV1BitstreamBuilder::AV1BitstreamBuilder() = default;
AV1BitstreamBuilder::~AV1BitstreamBuilder() = default;
AV1BitstreamBuilder::AV1BitstreamBuilder(AV1BitstreamBuilder&&) = default;

void AV1BitstreamBuilder::Write(uint64_t val, int num_bits) {
  queued_writes_.emplace_back(val, num_bits);
  total_outstanding_bits_ += num_bits;
}

void AV1BitstreamBuilder::WriteBool(bool val) {
  Write(val, 1);
}

std::vector<uint8_t> AV1BitstreamBuilder::Flush() && {
  std::vector<uint8_t> ret;
  uint8_t curr_byte = 0;
  int rem_bits_in_byte = 8;
  for (auto queued_write : queued_writes_) {
    uint64_t val = queued_write.first;
    int outstanding_bits = queued_write.second;
    while (outstanding_bits) {
      if (rem_bits_in_byte >= outstanding_bits) {
        curr_byte |= val << (rem_bits_in_byte - outstanding_bits);
        rem_bits_in_byte -= outstanding_bits;
        outstanding_bits = 0;
      } else {
        curr_byte |= (val >> (outstanding_bits - rem_bits_in_byte)) &
                     ((1 << rem_bits_in_byte) - 1);
        outstanding_bits -= rem_bits_in_byte;
        rem_bits_in_byte = 0;
      }
      if (!rem_bits_in_byte) {
        ret.push_back(curr_byte);
        curr_byte = 0;
        rem_bits_in_byte = 8;
      }
    }
  }

  if (rem_bits_in_byte != 8) {
    ret.push_back(curr_byte);
  }

  queued_writes_.clear();
  total_outstanding_bits_ = 0;

  return ret;
}

void AV1BitstreamBuilder::PutAlignBits() {
  int misalignment = total_outstanding_bits_ % 8;
  if (misalignment != 0) {
    int num_zero_bits = 8 - misalignment;
    Write(0, num_zero_bits);
  }
}

void AV1BitstreamBuilder::PutTrailingBits() {
  WriteBool(true);  // trialing one bit.
  PutAlignBits();
}

void AV1BitstreamBuilder::WriteOBUHeader(libgav1::ObuType type,
                                         bool extension_flag,
                                         bool has_size) {
  DCHECK_LE(1, type);
  DCHECK_LE(type, 8);
  WriteBool(false);  // forbidden bit must be set to 0.
  Write(static_cast<uint64_t>(type), 4);
  WriteBool(extension_flag);
  WriteBool(has_size);
  WriteBool(false);  // reserved bit must be set to 0.
}

// Encode a variable length unsigned integer of up to 4 bytes.
// Most significant bit of each byte indicates if parsing should continue, and
// the 7 least significant bits hold the actual data. So the encoded length
// may be 5 bytes under some circumstances.
// This function also has a fixed size mode where we pass in a fixed size for
// the data and the function zero pads up to that size.
// See section 4.10.5 of the AV1 specification.
void AV1BitstreamBuilder::WriteValueInLeb128(uint32_t value,
                                             std::optional<int> fixed_size) {
  for (int i = 0; i < fixed_size.value_or(5); i++) {
    uint8_t curr_byte = value & 0x7F;
    value >>= 7;
    if (value || fixed_size) {
      curr_byte |= 0x80;
      Write(curr_byte, 8);
    } else {
      Write(curr_byte, 8);
      break;
    }
  }
}

void AV1BitstreamBuilder::AppendBitstreamBuffer(AV1BitstreamBuilder buffer) {
  queued_writes_.insert(queued_writes_.end(),
                        std::make_move_iterator(buffer.queued_writes_.begin()),
                        std::make_move_iterator(buffer.queued_writes_.end()));
  total_outstanding_bits_ += buffer.total_outstanding_bits_;
}

}  // namespace media
