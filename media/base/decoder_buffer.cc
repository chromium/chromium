// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"

#include <sstream>

#include "base/containers/heap_array.h"
#include "base/debug/alias.h"
#include "media/base/subsample_entry.h"

namespace media {

DecoderBuffer::TimeInfo::TimeInfo() = default;
DecoderBuffer::TimeInfo::~TimeInfo() = default;
DecoderBuffer::TimeInfo::TimeInfo(const TimeInfo&) = default;
DecoderBuffer::TimeInfo& DecoderBuffer::TimeInfo::operator=(const TimeInfo&) =
    default;

DecoderBuffer::DecoderBuffer(size_t size) : size_(size) {
  Initialize();
}

DecoderBuffer::DecoderBuffer(base::span<const uint8_t> data)
    : size_(data.size()) {
  Initialize();
  data_.copy_from(data);
}

DecoderBuffer::DecoderBuffer(base::HeapArray<uint8_t> data)
    : data_(std::move(data)), size_(data_.size()) {}

DecoderBuffer::DecoderBuffer(base::ReadOnlySharedMemoryMapping mapping,
                             size_t size)
    : size_(size), read_only_mapping_(std::move(mapping)) {}

DecoderBuffer::DecoderBuffer(base::WritableSharedMemoryMapping mapping,
                             size_t size)
    : size_(size), writable_mapping_(std::move(mapping)) {}

DecoderBuffer::DecoderBuffer(std::unique_ptr<ExternalMemory> external_memory)
    : size_(external_memory->Span().size()),
      external_memory_(std::move(external_memory)) {}

DecoderBuffer::DecoderBuffer(DecoderBufferType decoder_buffer_type)
    : is_end_of_stream_(decoder_buffer_type ==
                        DecoderBufferType::kEndOfStream) {}

DecoderBuffer::~DecoderBuffer() = default;

void DecoderBuffer::Initialize() {
  data_ = base::HeapArray<uint8_t>::Uninit(size_);
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(
    base::span<const uint8_t> data) {
  return base::WrapRefCounted(new DecoderBuffer(data));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::FromArray(
    base::HeapArray<uint8_t> data) {
  return base::WrapRefCounted(new DecoderBuffer(std::move(data)));
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
  if (external_memory->Span().empty()) {
    return nullptr;
  }
  return base::WrapRefCounted(new DecoderBuffer(std::move(external_memory)));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CreateEOSBuffer() {
  return base::WrapRefCounted(
      new DecoderBuffer(DecoderBufferType::kEndOfStream));
}

// static
bool DecoderBuffer::DoSubsamplesMatch(const DecoderBuffer& buffer) {
  // If buffer is at end of stream, no subsamples to verify
  if (buffer.end_of_stream()) {
    return true;
  }

  // If stream is unencrypted, we do not have to verify subsamples size.
  if (!buffer.is_encrypted()) {
    return true;
  }

  const auto& subsamples = buffer.decrypt_config()->subsamples();
  if (subsamples.empty()) {
    return true;
  }
  return VerifySubsamplesMatchSize(subsamples, buffer.size());
}

base::span<const uint8_t> DecoderBuffer::AsSpan() const {
  DCHECK(!end_of_stream());
  if (read_only_mapping_.IsValid()) {
    return read_only_mapping_.GetMemoryAsSpan<const uint8_t>().first(size_);
  }
  if (writable_mapping_.IsValid()) {
    return writable_mapping_.GetMemoryAsSpan<const uint8_t>().first(size_);
  }
  if (external_memory_) {
    return external_memory_->Span().first(size_);
  }
  return data_.first(size_);
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
  return base::span(*this) == base::span(buffer);
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

size_t DecoderBuffer::GetMemoryUsage() const {
  size_t memory_usage = sizeof(DecoderBuffer);

  if (end_of_stream()) {
    return memory_usage;
  }

  memory_usage += size();

  // Side data and decrypt config would not change after construction.
  if (has_side_data()) {
    memory_usage += sizeof(decltype(side_data_->spatial_layers)::value_type) *
                    side_data_->spatial_layers.capacity();
    memory_usage += sizeof(decltype(side_data_->alpha_data)::value_type) *
                    side_data_->alpha_data.capacity();
  }
  if (decrypt_config_) {
    memory_usage += sizeof(DecryptConfig);
    memory_usage += decrypt_config_->key_id().capacity();
    memory_usage += decrypt_config_->iv().capacity();
    memory_usage +=
        sizeof(SubsampleEntry) * decrypt_config_->subsamples().capacity();
  }

  return memory_usage;
}

}  // namespace media
