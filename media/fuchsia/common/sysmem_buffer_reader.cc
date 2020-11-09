// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/common/sysmem_buffer_reader.h"

#include "base/fuchsia/fuchsia_logging.h"

namespace media {

SysmemBufferReader::SysmemBufferReader(
    fuchsia::sysmem::BufferCollectionInfo_2 info)
    : collection_info_(std::move(info)) {}

SysmemBufferReader::~SysmemBufferReader() = default;

bool SysmemBufferReader::Read(size_t index,
                              size_t offset,
                              base::span<uint8_t> data) {
  DCHECK_LT(index, num_buffers());
  DCHECK_LE(offset + data.size(),
            collection_info_.settings.buffer_settings.size_bytes);

  const fuchsia::sysmem::VmoBuffer& buffer = collection_info_.buffers[index];
  size_t vmo_offset = buffer.vmo_usable_start + offset;

  InvalidateCacheIfNecessary(buffer.vmo, vmo_offset, data.size());

  zx_status_t status = buffer.vmo.read(data.data(), vmo_offset, data.size());

  ZX_LOG_IF(ERROR, status != ZX_OK, status) << "Fail to read";
  return status == ZX_OK;
}

base::span<const uint8_t> SysmemBufferReader::GetMappingForBuffer(
    size_t index) {
  if (mappings_.empty())
    mappings_.resize(num_buffers());

  DCHECK_LT(index, mappings_.size());

  const fuchsia::sysmem::BufferMemorySettings& settings =
      collection_info_.settings.buffer_settings;
  fuchsia::sysmem::VmoBuffer& buffer = collection_info_.buffers[index];

  auto& mapping = mappings_[index];
  size_t buffer_start = buffer.vmo_usable_start;

  if (!mapping.IsValid()) {
    size_t mapping_size = buffer_start + settings.size_bytes;
    auto region = base::ReadOnlySharedMemoryRegion::Deserialize(
        base::subtle::PlatformSharedMemoryRegion::Take(
            std::move(buffer.vmo),
            base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly,
            mapping_size, base::UnguessableToken::Create()));

    mapping = region.Map();

    // Return the VMO handle back to buffer_.
    buffer.vmo = base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
                     std::move(region))
                     .PassPlatformHandle();
  }

  if (!mapping.IsValid()) {
    DLOG(WARNING) << "Failed to map VMO returned by sysmem";
    return {};
  }

  InvalidateCacheIfNecessary(buffer.vmo, buffer_start, settings.size_bytes);

  return base::make_span(
      reinterpret_cast<const uint8_t*>(mapping.memory()) + buffer_start,
      settings.size_bytes);
}

void SysmemBufferReader::InvalidateCacheIfNecessary(const zx::vmo& vmo,
                                                    size_t offset,
                                                    size_t size) {
  if (collection_info_.settings.buffer_settings.coherency_domain ==
      fuchsia::sysmem::CoherencyDomain::RAM) {
    zx_status_t status = vmo.op_range(ZX_VMO_OP_CACHE_CLEAN_INVALIDATE, offset,
                                      size, nullptr, 0);
    ZX_LOG_IF(ERROR, status != ZX_OK, status) << "Fail to invalidate cache";
  }
}

// static
std::unique_ptr<SysmemBufferReader> SysmemBufferReader::Create(
    fuchsia::sysmem::BufferCollectionInfo_2 info) {
  return std::make_unique<SysmemBufferReader>(std::move(info));
}

// static
fuchsia::sysmem::BufferCollectionConstraints
SysmemBufferReader::GetRecommendedConstraints(
    size_t min_buffer_count,
    base::Optional<size_t> min_buffer_size) {
  fuchsia::sysmem::BufferCollectionConstraints buffer_constraints;
  buffer_constraints.usage.cpu = fuchsia::sysmem::cpuUsageRead;
  buffer_constraints.min_buffer_count = min_buffer_count;
  if (min_buffer_size) {
    buffer_constraints.has_buffer_memory_constraints = true;
    buffer_constraints.buffer_memory_constraints.min_size_bytes =
        min_buffer_size.value();
    buffer_constraints.buffer_memory_constraints.ram_domain_supported = true;
    buffer_constraints.buffer_memory_constraints.cpu_domain_supported = true;
  }
  return buffer_constraints;
}

}  // namespace media
