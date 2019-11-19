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
  DCHECK_LT(index, collection_info_.buffer_count);
  const fuchsia::sysmem::BufferMemorySettings& settings =
      collection_info_.settings.buffer_settings;
  fuchsia::sysmem::VmoBuffer& buffer = collection_info_.buffers[index];
  DCHECK_LE(buffer.vmo_usable_start + offset + data.size(),
            settings.size_bytes);

  size_t vmo_offset = buffer.vmo_usable_start + offset;

  // Invalidate cache.
  if (settings.coherency_domain == fuchsia::sysmem::CoherencyDomain::RAM) {
    zx_status_t status = buffer.vmo.op_range(
        ZX_VMO_OP_CACHE_CLEAN_INVALIDATE, vmo_offset, data.size(), nullptr, 0);
    ZX_LOG_IF(ERROR, status != ZX_OK, status) << "Fail to invalidate cache";
  }

  zx_status_t status = buffer.vmo.read(data.data(), vmo_offset, data.size());
  ZX_LOG_IF(ERROR, status != ZX_OK, status) << "Fail to read";
  return status == ZX_OK;
}

// static
std::unique_ptr<SysmemBufferReader> SysmemBufferReader::Create(
    fuchsia::sysmem::BufferCollectionInfo_2 info) {
  return std::make_unique<SysmemBufferReader>(std::move(info));
}

// static
fuchsia::sysmem::BufferCollectionConstraints
SysmemBufferReader::GetRecommendedConstraints(size_t max_used_output_frames) {
  fuchsia::sysmem::BufferCollectionConstraints buffer_constraints;
  buffer_constraints.usage.cpu = fuchsia::sysmem::cpuUsageRead;
  buffer_constraints.min_buffer_count_for_camping = max_used_output_frames;
  return buffer_constraints;
}

}  // namespace media
