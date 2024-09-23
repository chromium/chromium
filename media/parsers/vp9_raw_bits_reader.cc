// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/vp9_raw_bits_reader.h"

#include <limits.h>

#include <memory>

#include "base/check_op.h"
#include "media/base/bit_reader.h"

namespace media {

Vp9RawBitsReader::Vp9RawBitsReader() : valid_(true) {}

Vp9RawBitsReader::~Vp9RawBitsReader() = default;

void Vp9RawBitsReader::Initialize(const uint8_t* data, size_t size) {
  DCHECK(data);
  reader_ = std::make_unique<BitReader>(data, size);
  valid_ = true;
}

bool Vp9RawBitsReader::ReadBool() {
  DCHECK(reader_);
  if (!valid_)
    return false;

  int value = 0;
  valid_ = reader_->ReadBits(1, &value);
  return valid_ ? value == 1 : false;
}

int Vp9RawBitsReader::ReadLiteral(int bits) {
  DCHECK(reader_);
  if (!valid_)
    return 0;

  int value = 0;
  DCHECK_LT(static_cast<size_t>(bits), sizeof(value) * 8);
  valid_ = reader_->ReadBits(bits, &value);
  return valid_ ? value : 0;
}

int Vp9RawBitsReader::ReadSignedLiteral(int bits) {
  int value = ReadLiteral(bits);
  return ReadBool() ? -value : value;
}

size_t Vp9RawBitsReader::GetBytesRead() const {
  DCHECK(reader_);
  return (reader_->bits_read() + 7) / 8;
}

bool Vp9RawBitsReader::ConsumeTrailingBits() {
  DCHECK(reader_);
  int bits_left = GetBytesRead() * 8 - reader_->bits_read();
  return ReadLiteral(bits_left) == 0;
}

}  // namespace media
