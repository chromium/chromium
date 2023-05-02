// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/box_byte_stream.h"
#include "base/logging.h"

namespace media {

BoxByteStream::BoxByteStream() : buffer_(kDefaultBufferLimit) {
  writer_.emplace(reinterpret_cast<char*>(buffer_.data()), buffer_.size());
}

BoxByteStream::~BoxByteStream() {
  DCHECK(!writer_);
  DCHECK(size_offsets_.empty());
}

void BoxByteStream::WritePlaceholderSizeU32() {
  size_offsets_.push_back(position_);
  WriteU32(0);
}

void BoxByteStream::WriteU8(uint8_t value) {
  while (!writer_->WriteU8(value)) {
    GrowWriter();
  }
  position_ += 1;
}

void BoxByteStream::WriteU16(uint16_t value) {
  while (!writer_->WriteU16(value)) {
    GrowWriter();
  }
  position_ += 2;
}

void BoxByteStream::WriteU24(uint32_t value) {
  CHECK_LE(value, 0xffffffu);

  while (!writer_->WriteBytes(&value, 3)) {
    GrowWriter();
  }
  position_ += 3;
}

void BoxByteStream::WriteU32(uint32_t value) {
  while (!writer_->WriteU32(value)) {
    GrowWriter();
  }
  position_ += 4;
}

void BoxByteStream::WriteU64(uint64_t value) {
  while (!writer_->WriteU64(value)) {
    GrowWriter();
  }
  position_ += 8;
}

std::vector<uint8_t> BoxByteStream::EndWrite() {
  buffer_.resize(position_);
  for (size_t size_offset : size_offsets_) {
    base::BigEndianWriter size_writer(
        reinterpret_cast<char*>(&buffer_[size_offset]), 4);
    size_writer.WriteU32(position_ - size_offset);
  }
  writer_.reset();
  size_offsets_.clear();
  return std::move(buffer_);
}

void BoxByteStream::GrowWriter() {
  buffer_.resize(buffer_.size() * 1.5);
  writer_.emplace(reinterpret_cast<char*>(buffer_.data()), buffer_.size());
  writer_->Skip(position_);
}

}  // namespace media
