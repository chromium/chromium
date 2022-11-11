// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"

#include <sstream>

#include "base/debug/alias.h"

namespace media {

DecoderBuffer::TimeInfo::TimeInfo() = default;
DecoderBuffer::TimeInfo::~TimeInfo() = default;
DecoderBuffer::TimeInfo::TimeInfo(const TimeInfo&) = default;
DecoderBuffer::TimeInfo& DecoderBuffer::TimeInfo::operator=(const TimeInfo&) =
    default;

DecoderBuffer::DecoderBuffer(size_t size)
    : size_(size), side_data_size_(0), is_key_frame_(false) {
  Initialize();
}

DecoderBuffer::DecoderBuffer(const uint8_t* data,
                             size_t size,
                             const uint8_t* side_data,
                             size_t side_data_size)
    : size_(size), side_data_size_(side_data_size), is_key_frame_(false) {
  if (!data) {
    CHECK_EQ(size_, 0u);
    CHECK(!side_data);
    return;
  }

  Initialize();

  memcpy(data_.get(), data, size_);

  if (!side_data) {
    CHECK_EQ(side_data_size, 0u);
    return;
  }

  DCHECK_GT(side_data_size_, 0u);
  memcpy(side_data_.get(), side_data, side_data_size_);
}

DecoderBuffer::DecoderBuffer(std::unique_ptr<uint8_t[]> data, size_t size)
    : data_(std::move(data)), size_(size) {}

DecoderBuffer::DecoderBuffer(base::ReadOnlySharedMemoryMapping mapping,
                             size_t size)
    : size_(size), read_only_mapping_(std::move(mapping)) {}

DecoderBuffer::DecoderBuffer(base::WritableSharedMemoryMapping mapping,
                             size_t size)
    : size_(size), writable_mapping_(std::move(mapping)) {}

DecoderBuffer::DecoderBuffer(std::unique_ptr<ExternalMemory> external_memory)
    : size_(external_memory->span().size()),
      external_memory_(std::move(external_memory)) {}

DecoderBuffer::~DecoderBuffer() {
  data_.reset();
  side_data_.reset();
}

void DecoderBuffer::Initialize() {
  data_.reset(new uint8_t[size_]);
  if (side_data_size_ > 0)
    side_data_.reset(new uint8_t[side_data_size_]);
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(const uint8_t* data,
                                                     size_t data_size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return base::WrapRefCounted(new DecoderBuffer(data, data_size, nullptr, 0));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(const uint8_t* data,
                                                     size_t data_size,
                                                     const uint8_t* side_data,
                                                     size_t side_data_size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  CHECK(side_data);
  return base::WrapRefCounted(
      new DecoderBuffer(data, data_size, side_data, side_data_size));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::FromArray(
    std::unique_ptr<uint8_t[]> data,
    size_t size) {
  CHECK(data);
  return base::WrapRefCounted(new DecoderBuffer(std::move(data), size));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::FromSharedMemoryRegion(
    base::UnsafeSharedMemoryRegion region,
    uint64_t offset,
    size_t size) {
  if (size == 0) {
    return nullptr;
  }

  auto mapping = region.MapAt(offset, size);
  if (!mapping.IsValid()) {
    return nullptr;
  }
  return base::WrapRefCounted(new DecoderBuffer(std::move(mapping), size));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::FromSharedMemoryRegion(
    base::ReadOnlySharedMemoryRegion region,
    uint64_t offset,
    size_t size) {
  if (size == 0) {
    return nullptr;
  }
  auto mapping = region.MapAt(offset, size);
  if (!mapping.IsValid()) {
    return nullptr;
  }
  return base::WrapRefCounted(new DecoderBuffer(std::move(mapping), size));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::FromExternalMemory(
    std::unique_ptr<ExternalMemory> external_memory) {
  DCHECK(external_memory);
  if (external_memory->span().empty())
    return nullptr;
  return base::WrapRefCounted(new DecoderBuffer(std::move(external_memory)));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CreateEOSBuffer() {
  return base::WrapRefCounted(new DecoderBuffer(nullptr, 0, nullptr, 0));
}

bool DecoderBuffer::MatchesMetadataForTesting(
    const DecoderBuffer& buffer) const {
  if (end_of_stream() != buffer.end_of_stream())
    return false;

  // It is illegal to call any member function if eos is true.
  if (end_of_stream())
    return true;

  if (timestamp() != buffer.timestamp() || duration() != buffer.duration() ||
      is_key_frame() != buffer.is_key_frame() ||
      discard_padding() != buffer.discard_padding()) {
    return false;
  }

  if ((decrypt_config() == nullptr) != (buffer.decrypt_config() == nullptr))
    return false;

  return decrypt_config() ? decrypt_config()->Matches(*buffer.decrypt_config())
                          : true;
}

bool DecoderBuffer::MatchesForTesting(const DecoderBuffer& buffer) const {
  if (!MatchesMetadataForTesting(buffer))  // IN-TEST
    return false;

  // It is illegal to call any member function if eos is true.
  if (end_of_stream())
    return true;

  DCHECK(!buffer.end_of_stream());
  return data_size() == buffer.data_size() &&
         side_data_size() == buffer.side_data_size() &&
         memcmp(data(), buffer.data(), data_size()) == 0 &&
         memcmp(side_data(), buffer.side_data(), side_data_size()) == 0;
}

std::string DecoderBuffer::AsHumanReadableString(bool verbose) const {
  if (end_of_stream())
    return "EOS";

  std::ostringstream s;

  s << "{timestamp=" << time_info_.timestamp.InMicroseconds()
    << " duration=" << time_info_.duration.InMicroseconds() << " size=" << size_
    << " is_key_frame=" << is_key_frame_
    << " encrypted=" << (decrypt_config_ != nullptr);

  if (verbose) {
    s << " side_data_size=" << side_data_size_ << " discard_padding (us)=("
      << time_info_.discard_padding.first.InMicroseconds() << ", "
      << time_info_.discard_padding.second.InMicroseconds() << ")";

    if (decrypt_config_)
      s << " decrypt_config=" << (*decrypt_config_);
  }

  s << "}";

  return s.str();
}

void DecoderBuffer::set_timestamp(base::TimeDelta timestamp) {
  DCHECK(!end_of_stream());
  time_info_.timestamp = timestamp;
}

void DecoderBuffer::CopySideDataFrom(const uint8_t* side_data,
                                     size_t side_data_size) {
  if (side_data_size > 0) {
    side_data_size_ = side_data_size;
    side_data_.reset(new uint8_t[side_data_size_]);
    memcpy(side_data_.get(), side_data, side_data_size_);
  } else {
    side_data_.reset();
    side_data_size_ = 0;
  }
}

}  // namespace media
