// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"

#include <sstream>

#include "base/containers/heap_array.h"
#include "base/debug/alias.h"
#include "media/base/subsample_entry.h"

namespace media {

namespace {

template <class T>
class ExternalSharedMemoryAdapter : public DecoderBuffer::ExternalMemory {
 public:
  explicit ExternalSharedMemoryAdapter(T mapping)
      : mapping_(std::move(mapping)) {}

  const base::span<const uint8_t> Span() const override {
    return mapping_.template GetMemoryAsSpan<const uint8_t>();
  }

 private:
  T mapping_;
};

}  // namespace

DecoderBuffer::DecoderBuffer(size_t size)
    : data_(base::HeapArray<uint8_t>::Uninit(size)) {}

DecoderBuffer::DecoderBuffer(base::span<const uint8_t> data)
    : data_(base::HeapArray<uint8_t>::CopiedFrom(data)) {}

DecoderBuffer::DecoderBuffer(base::HeapArray<uint8_t> data)
    : data_(std::move(data)) {}

DecoderBuffer::DecoderBuffer(std::unique_ptr<ExternalMemory> external_memory)
    : external_memory_(std::move(external_memory)) {}

DecoderBuffer::DecoderBuffer(DecoderBufferType decoder_buffer_type,
                             std::optional<ConfigVariant> next_config)
    : is_end_of_stream_(decoder_buffer_type ==
                        DecoderBufferType::kEndOfStream) {
  if (next_config) {
    DCHECK(end_of_stream());
    side_data_ = std::make_unique<DecoderBufferSideData>();
    side_data_->next_config = std::move(next_config);
  }
}

DecoderBuffer::~DecoderBuffer() = default;

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

  return FromExternalMemory(
      std::make_unique<
          ExternalSharedMemoryAdapter<base::WritableSharedMemoryMapping>>(
          std::move(mapping)));
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

  return FromExternalMemory(
      std::make_unique<
          ExternalSharedMemoryAdapter<base::ReadOnlySharedMemoryMapping>>(
          std::move(mapping)));
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
scoped_refptr<DecoderBuffer> DecoderBuffer::CreateEOSBuffer(
    std::optional<ConfigVariant> next_config) {
  return base::WrapRefCounted(new DecoderBuffer(DecoderBufferType::kEndOfStream,
                                                std::move(next_config)));
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
  return external_memory_ ? external_memory_->Span() : data_;
}

void DecoderBuffer::set_discard_padding(const DiscardPadding& discard_padding) {
  DCHECK(!end_of_stream());
  if (!side_data_ && discard_padding == DiscardPadding()) {
    return;
  }
  WritableSideData().discard_padding = discard_padding;
}

DecoderBufferSideData& DecoderBuffer::WritableSideData() {
  DCHECK(!end_of_stream());
  if (!has_side_data()) {
    side_data_ = std::make_unique<DecoderBufferSideData>();
  }
  return *side_data_;
}

void DecoderBuffer::set_side_data(
    std::optional<DecoderBufferSideData> side_data) {
  DCHECK(!end_of_stream());
  if (!side_data) {
    side_data_.reset();
    return;
  }
  WritableSideData() = *side_data;
}

bool DecoderBuffer::MatchesMetadataForTesting(
    const DecoderBuffer& buffer) const {
  if (end_of_stream() != buffer.end_of_stream()) {
    return false;
  }

  if (has_side_data() != buffer.has_side_data()) {
    return false;
  }

  // Note: We use `side_data_` directly to avoid DCHECKs for EOS buffers.
  if (has_side_data() && !side_data_->Matches(*buffer.side_data_)) {
    return false;
  }

  // None of the following methods may be called on an EOS buffer.
  if (end_of_stream()) {
    return true;
  }

  if (timestamp() != buffer.timestamp() || duration() != buffer.duration() ||
      is_key_frame() != buffer.is_key_frame()) {
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
  if (end_of_stream()) {
    if (!next_config()) {
      return "EOS";
    }

    std::string config;
    const auto nc = next_config().value();
    if (const auto* ac = absl::get_if<media::AudioDecoderConfig>(&nc)) {
      config = ac->AsHumanReadableString();
    } else {
      config = absl::get<media::VideoDecoderConfig>(nc).AsHumanReadableString();
    }

    return base::StringPrintf("EOS config=(%s)", config.c_str());
  }

  std::ostringstream s;

  s << "{timestamp=" << timestamp_.InMicroseconds()
    << " duration=" << duration_.InMicroseconds() << " size=" << size()
    << " is_key_frame=" << is_key_frame_
    << " encrypted=" << (decrypt_config_ != nullptr);

  if (verbose) {
    s << " has_side_data=" << has_side_data();
    if (has_side_data()) {
      s << " discard_padding (us)=("
        << side_data_->discard_padding.first.InMicroseconds() << ", "
        << side_data_->discard_padding.second.InMicroseconds() << ")";
    }
    if (decrypt_config_) {
      s << " decrypt_config=" << (*decrypt_config_);
    }
  }

  s << "}";

  return s.str();
}

void DecoderBuffer::set_timestamp(base::TimeDelta timestamp) {
  DCHECK(!end_of_stream());
  timestamp_ = timestamp;
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
