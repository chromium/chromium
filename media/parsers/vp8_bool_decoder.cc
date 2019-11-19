// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

/*
 * Copyright (c) 2010, The WebM Project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google, nor the WebM Project, nor the names
 *     of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file is modified from the dboolhuff.{c,h} from the WebM's libvpx
// project. (http://www.webmproject.org/code)
// It is used to decode bits from a vp8 stream.

#include "media/parsers/vp8_bool_decoder.h"

#include <limits.h>

#include <algorithm>

#include "base/numerics/safe_conversions.h"

namespace media {

#define VP8_BD_VALUE_BIT \
  static_cast<int>(sizeof(Vp8BoolDecoder::value_) * CHAR_BIT)

static const int kDefaultProbability = 0x80;  // 0x80 / 256 = 0.5

// This is meant to be a large, positive constant that can still be efficiently
// loaded as an immediate (on platforms like ARM, for example). Even relatively
// modest values like 100 would work fine.
#define VP8_LOTS_OF_BITS (0x40000000)

// The number of leading zeros.
static const unsigned char kVp8Norm[256] = {
    0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

Vp8BoolDecoder::Vp8BoolDecoder()
    : user_buffer_(NULL),
      user_buffer_end_(NULL),
      value_(0),
      count_(-8),
      range_(255) {
}

bool Vp8BoolDecoder::Initialize(const uint8_t* data, size_t size) {
  if (data == NULL || size == 0)
    return false;
  user_buffer_start_ = data;
  user_buffer_ = data;
  user_buffer_end_ = data + size;
  value_ = 0;
  count_ = -8;
  range_ = 255;
  return true;
}

void Vp8BoolDecoder::FillDecoder() {
  DCHECK(user_buffer_ != NULL);
  int shift = VP8_BD_VALUE_BIT - CHAR_BIT - (count_ + CHAR_BIT);
  size_t bytes_left = user_buffer_end_ - user_buffer_;
  size_t bits_left = bytes_left * CHAR_BIT;
  int x = static_cast<int>(shift + CHAR_BIT - bits_left);
  int loop_end = 0;

  if (x >= 0) {
    count_ += VP8_LOTS_OF_BITS;
    loop_end = x;
  }

  if (x < 0 || bits_left) {
    while (shift >= loop_end) {
      count_ += CHAR_BIT;
      value_ |= static_cast<size_t>(*user_buffer_) << shift;
      ++user_buffer_;
      shift -= CHAR_BIT;
    }
  }
}

int Vp8BoolDecoder::ReadBit(int probability) {
  int bit = 0;
  size_t split = 1 + (((range_ - 1) * probability) >> 8);
  if (count_ < 0)
    FillDecoder();
  size_t bigsplit = static_cast<size_t>(split) << (VP8_BD_VALUE_BIT - 8);

  if (value_ >= bigsplit) {
    range_ -= split;
    value_ -= bigsplit;
    bit = 1;
  } else {
    range_ = split;
  }

  size_t shift = kVp8Norm[range_];
  range_ <<= shift;
  value_ <<= shift;
  count_ -= shift;

  DCHECK_EQ(1U, (range_ >> 7));  // In the range [128, 255].

  return bit;
}

bool Vp8BoolDecoder::ReadLiteral(size_t num_bits, int* out) {
  DCHECK_LE(num_bits, sizeof(int) * CHAR_BIT);
  *out = 0;
  for (; num_bits > 0; --num_bits)
    *out = (*out << 1) | ReadBit(kDefaultProbability);
  return !OutOfBuffer();
}

bool Vp8BoolDecoder::ReadBool(bool* out, uint8_t probability) {
  *out = !!ReadBit(probability);
  return !OutOfBuffer();
}

bool Vp8BoolDecoder::ReadBool(bool* out) {
  return ReadBool(out, kDefaultProbability);
}

bool Vp8BoolDecoder::ReadLiteralWithSign(size_t num_bits, int* out) {
  ReadLiteral(num_bits, out);
  // Read sign.
  if (ReadBit(kDefaultProbability))
    *out = -*out;
  return !OutOfBuffer();
}

size_t Vp8BoolDecoder::BitOffset() {
  int bit_count = count_ + 8;
  if (bit_count > VP8_BD_VALUE_BIT)
    // Capped at 0 to ignore buffer underrun.
    bit_count = std::max(0, bit_count - VP8_LOTS_OF_BITS);
  return (user_buffer_ - user_buffer_start_) * 8 - bit_count;
}

uint8_t Vp8BoolDecoder::GetRange() {
  return base::checked_cast<uint8_t>(range_);
}

uint8_t Vp8BoolDecoder::GetBottom() {
  if (count_ < 0)
    FillDecoder();
  return static_cast<uint8_t>(value_ >> (VP8_BD_VALUE_BIT - 8));
}

inline bool Vp8BoolDecoder::OutOfBuffer() {
  // Check if we have reached the end of the buffer.
  //
  // Variable |count_| stores the number of bits in the |value_| buffer, minus
  // 8. The top byte is part of the algorithm and the remainder is buffered to
  // be shifted into it. So, if |count_| == 8, the top 16 bits of |value_| are
  // occupied, 8 for the algorithm and 8 in the buffer.
  //
  // When reading a byte from the user's buffer, |count_| is filled with 8 and
  // one byte is filled into the |value_| buffer. When we reach the end of the
  // data, |count_| is additionally filled with VP8_LOTS_OF_BITS. So when
  // |count_| == VP8_LOTS_OF_BITS - 1, the user's data has been exhausted.
  return (count_ > VP8_BD_VALUE_BIT) && (count_ < VP8_LOTS_OF_BITS);
}

}  // namespace media
