// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"

#include <sstream>

#include "base/debug/alias.h"
#include "media/base/subsample_entry.h"

namespace media {

DecoderBuffer::TimeInfo::TimeInfo() = default;
DecoderBuffer::TimeInfo::~TimeInfo() = default;
DecoderBuffer::TimeInfo::TimeInfo(const TimeInfo&) = default;
DecoderBuffer::TimeInfo& DecoderBuffer::TimeInfo::operator=(const TimeInfo&) =
    default;

DecoderBuffer::DecoderBuffer(size_t size) : size_(size), is_key_frame_(false) {
  Initialize();
}

DecoderBuffer::DecoderBuffer(const uint8_t* data, size_t size)
    : size_(size), is_key_frame_(false) {
  if (!data) {
    CHECK_EQ(size_, 0u);
    return;
  }

  Initialize();

  memcpy(data_.get(), data, size_);
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
}

void DecoderBuffer::Initialize() {
  data_.reset(new uint8_t[size_]);
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(const uint8_t* data,
                                                     size_t data_size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return base::WrapRefCounted(new DecoderBuffer(data, data_size));
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
  return base::WrapRefCounted(new DecoderBuffer(nullptr, 0));
}

// static
bool DecoderBuffer::DoSubsamplesMatch(const DecoderBuffer& encrypted) {
  // If buffer is at end of stream, no subsamples to verify
  if (encrypted.end_of_stream()) {
    return true;
  }

  // If stream is unencrypted, we do not have to verify subsamples size.
  const DecryptConfig* decrypt_config = encrypted.decrypt_config();
  if (decrypt_config == nullptr ||
      decrypt_config->encryption_scheme() == EncryptionScheme::kUnencrypted) {
    return true;
  }

  const auto& subsamples = decrypt_config->subsamples();
  if (subsamples.empty()) {
    return true;
  }
  return VerifySubsamplesMatchSize(subsamples, encrypted.data_size());
}

DecoderBufferSideData& DecoderBuffer::WritableSideData() {
  if (!side_data_.has_value()) {
    side_data_.emplace();
  }
  return side_data_.value();
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

  if (has_side_data() != buffer.has_side_data()) {
    return false;
  }

  if (has_side_data() && !side_data()->Matches(buffer.side_data().value())) {
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
         memcmp(data(), buffer.data(), data_size()) == 0;
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
    s << " has_side_data=" << has_side_data() << " discard_padding (us)=("
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

}  // namespace media
