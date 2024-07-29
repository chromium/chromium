// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/vp9_bool_decoder.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "media/base/bit_reader.h"

namespace media {

namespace {

// This is an optimization lookup table for the loop in spec 9.2.2.
//     while BoolRange <= 128:
//       read 1 bit
//       BoolRange *= 2
// This table indicates how many iterations to run for a given BoolRange. So
// the loop could be reduced to
//     read (kCountToShiftTo128[BoolRange]) bits
const int kCountToShiftTo128[256] = {
    0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
}  // namespace

Vp9BoolDecoder::Vp9BoolDecoder() = default;

Vp9BoolDecoder::~Vp9BoolDecoder() = default;

// 9.2.1 Initialization process for Boolean decoder
bool Vp9BoolDecoder::Initialize(const uint8_t* data, size_t size) {
  DCHECK(data);
  if (size < 1) {
    DVLOG(1) << "input size of bool decoder shall be at least 1";
    valid_ = false;
    return false;
  }

  reader_ = std::make_unique<BitReader>(data, size);
  valid_ = true;

  bool_value_ = 0;
  count_to_fill_ = 8;
  bool_range_ = 255;
  if (ReadLiteral(1) != 0) {
    DVLOG(1) << "marker bit should be 0";
    valid_ = false;
    return false;
  }
  return true;
}

// Fill at least |count_to_fill_| bits and prefill remain bits of |bool_value_|
// if data is enough.
bool Vp9BoolDecoder::Fill() {
  DCHECK_GE(count_to_fill_, 0);

  int bits_left = reader_->bits_available();
  if (bits_left < count_to_fill_) {
    valid_ = false;
    DVLOG(1) << "Vp9BoolDecoder reads beyond the end of stream";
    return false;
  }

  DCHECK_LE(count_to_fill_, kBoolSize);
  int max_bits_to_read = kBigBoolBitSize - kBoolSize + count_to_fill_;
  int bits_to_read = std::min(max_bits_to_read, bits_left);

  BigBool data;
  reader_->ReadBits(bits_to_read, &data);
  bool_value_ |= data << (max_bits_to_read - bits_to_read);
  count_to_fill_ -= bits_to_read;

  return true;
}

// 9.2.2 Boolean decoding process
bool Vp9BoolDecoder::ReadBool(int prob) {
  DCHECK(reader_);

  if (count_to_fill_ > 0) {
    if (!Fill())
      return false;
  }

  unsigned int split = (bool_range_ * prob + (256 - prob)) >> kBoolSize;
  BigBool big_split = static_cast<BigBool>(split)
                      << (kBigBoolBitSize - kBoolSize);

  bool bit;
  if (bool_value_ < big_split) {
    bool_range_ = split;
    bit = false;
  } else {
    bool_range_ -= split;
    bool_value_ -= big_split;
    bit = true;
  }

  // Need to fill |count| bits next time in order to make |bool_range_| >=
  // 128.
  DCHECK_LT(bool_range_, std::size(kCountToShiftTo128));
  DCHECK_GT(bool_range_, 0u);
  int count = kCountToShiftTo128[bool_range_];
  bool_range_ <<= count;
  bool_value_ <<= count;
  count_to_fill_ += count;

  return bit;
}

// 9.2.4 Parsing process for read_literal
uint8_t Vp9BoolDecoder::ReadLiteral(int bits) {
  DCHECK_LT(static_cast<size_t>(bits), sizeof(uint8_t) * 8);
  DCHECK(reader_);

  uint8_t x = 0;
  for (int i = 0; i < bits; i++)
    x = 2 * x + ReadBool(128);

  return x;
}

bool Vp9BoolDecoder::ConsumePaddingBits() {
  DCHECK(reader_);

  if (count_to_fill_ > reader_->bits_available()) {
    // 9.2.2 Boolean decoding process
    // Although we actually don't used the value, spec says the bitstream
    // should have enough bits to fill bool range, this should never happen.
    DVLOG(2) << "not enough bits in bitstream to fill bool range";
    return false;
  }

  if (bool_value_ != 0) {
    DVLOG(1) << "prefilled padding bits are not zero";
    return false;
  }
  while (reader_->bits_available() > 0) {
    int data;
    int size_to_read =
        std::min(reader_->bits_available(), static_cast<int>(sizeof(data) * 8));
    reader_->ReadBits(size_to_read, &data);
    if (data != 0) {
      DVLOG(1) << "padding bits are not zero";
      return false;
    }
  }
  return true;
}

}  // namespace media
