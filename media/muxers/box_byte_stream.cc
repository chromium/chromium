// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/box_byte_stream.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"

namespace media {

namespace {

void WriteSize(size_t value, base::span<uint8_t, 4u> data) {
  data.copy_from(base::U32ToBigEndian(base::checked_cast<uint32_t>(value)));
}

}  // namespace

BoxByteStream::BoxByteStream() : buffer_(kDefaultBufferLimit) {
  writer_.emplace(buffer_);
}

BoxByteStream::~BoxByteStream() {
  DCHECK(!writer_);
  DCHECK(!has_open_boxes());
}

void BoxByteStream::StartBox(mp4::FourCC fourcc) {
  CHECK(!buffer_.empty());
  size_offsets_.push_back(position_);
  WriteU32(0);

  WriteU32(fourcc);
}

void BoxByteStream::StartFullBox(mp4::FourCC fourcc,
                                 uint32_t flags,
                                 uint8_t version) {
  StartBox(fourcc);
  uint32_t value = version << 24 | (flags & 0xffffff);
  WriteU32(value);
}

void BoxByteStream::WriteU8(uint8_t value) {
  CHECK(!buffer_.empty());
  while (!writer_->WriteU8BigEndian(value)) {
    GrowWriter();
  }
  position_ += 1;
}

void BoxByteStream::WriteU16(uint16_t value) {
  while (!writer_->WriteU16BigEndian(value)) {
    GrowWriter();
  }
  position_ += 2;
}

void BoxByteStream::WriteU32(uint32_t value) {
  CHECK(!buffer_.empty());
  while (!writer_->WriteU32BigEndian(value)) {
    GrowWriter();
  }
  position_ += 4;
}

void BoxByteStream::WriteU64(uint64_t value) {
  CHECK(!buffer_.empty());
  while (!writer_->WriteU64BigEndian(value)) {
    GrowWriter();
  }
  position_ += 8;
}

void BoxByteStream::WriteBytes(const void* buf, size_t len) {
  CHECK(!buffer_.empty());
  while (!writer_->Write(
      // TODO(crbug.com/40284755): The caller must have provided a valid buf/len
      // pair. This method should receive a span instead of a pointer.
      UNSAFE_TODO(base::span(static_cast<const uint8_t*>(buf), len)))) {
    GrowWriter();
  }
  position_ += len;
}

void BoxByteStream::WriteString(std::string_view value) {
  if (value.empty()) {
    WriteU8(0);
    return;
  }

  WriteBytes(value.data(), value.size());

  // Ensure null terminated string.
  if (value.back() != 0) {
    WriteU8(0);
  }
}

std::vector<uint8_t> BoxByteStream::Flush() {
  CHECK(!buffer_.empty());
  DCHECK(!has_open_boxes());

  buffer_.resize(position_);

  writer_.reset();
  size_offsets_.clear();
  return std::move(buffer_);
}

void BoxByteStream::EndBox() {
  CHECK(!buffer_.empty());
  CHECK(!size_offsets_.empty());

  size_t size_offset = size_offsets_.back();
  size_offsets_.pop_back();

  WriteSize(position_ - size_offset,
            base::span(buffer_).subspan(size_offset).first<4>());
}

void BoxByteStream::WriteOffsetPlaceholder() {
  data_offsets_by_track_.push(position_);
  WriteU32(0);
}

void BoxByteStream::FlushCurrentOffset() {
  CHECK(!data_offsets_by_track_.empty());

  size_t offset_in_trun = data_offsets_by_track_.front();
  data_offsets_by_track_.pop();

  WriteSize(position_, base::span(buffer_).subspan(offset_in_trun).first<4>());
}

void BoxByteStream::GrowWriter() {
  CHECK(!buffer_.empty());
  // `writer_` points into `buffer_` so destroy and recreate it when
  // invalidating its pointer by resizing `buffer_`.
  writer_.reset();
  buffer_.resize(buffer_.size() * 1.5);
  writer_.emplace(buffer_);
  writer_->Skip(position_);
}

}  // namespace media
