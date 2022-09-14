// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bitstream_buffer.h"

#include "base/numerics/checked_math.h"
#include "media/base/decrypt_config.h"

namespace media {

BitstreamBuffer::BitstreamBuffer()
    : BitstreamBuffer(-1, base::UnsafeSharedMemoryRegion(), 0) {}

BitstreamBuffer::BitstreamBuffer(int32_t id,
                                 base::UnsafeSharedMemoryRegion region,
                                 size_t size,
                                 uint64_t offset,
                                 base::TimeDelta presentation_timestamp)
    : id_(id),
      region_(std::move(region)),
      size_(size),
      offset_(offset),
      presentation_timestamp_(presentation_timestamp) {}

BitstreamBuffer::BitstreamBuffer(BitstreamBuffer&&) = default;
BitstreamBuffer& BitstreamBuffer::operator=(BitstreamBuffer&&) = default;

BitstreamBuffer::~BitstreamBuffer() = default;

scoped_refptr<DecoderBuffer> BitstreamBuffer::ToDecoderBuffer() {
  return ToDecoderBuffer(0, size_);
}
scoped_refptr<DecoderBuffer> BitstreamBuffer::ToDecoderBuffer(off_t offset,
                                                              size_t size) {
  // We do allow mapping beyond the bounds of the original specified size in
  // order to deal with the OEMCrypto secure buffer format, but we do ensure
  // it stays within the shared memory region.
  base::CheckedNumeric<off_t> total_range(offset_);
  total_range += offset;
  if (!total_range.IsValid())
    return nullptr;
  const off_t final_offset = total_range.ValueOrDie();
  total_range += base::checked_cast<off_t>(size);
  if (!total_range.IsValid<size_t>())
    return nullptr;
  if (total_range.ValueOrDie<size_t>() > region_.GetSize())
    return nullptr;
  scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::FromSharedMemoryRegion(
      std::move(region_), final_offset, size);
  if (!buffer)
    return nullptr;
  buffer->set_timestamp(presentation_timestamp_);
  if (!key_id_.empty()) {
    buffer->set_decrypt_config(
        DecryptConfig::CreateCencConfig(key_id_, iv_, subsamples_));
  }
  return buffer;
}

void BitstreamBuffer::SetDecryptionSettings(
    const std::string& key_id,
    const std::string& iv,
    const std::vector<SubsampleEntry>& subsamples) {
  key_id_ = key_id;
  iv_ = iv;
  subsamples_ = subsamples;
}

}  // namespace media
