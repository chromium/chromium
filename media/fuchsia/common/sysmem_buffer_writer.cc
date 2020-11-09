// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/common/sysmem_buffer_writer.h"

#include <zircon/rights.h>
#include <algorithm>

#include "base/bits.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/process/process_metrics.h"

namespace media {

class SysmemBufferWriter::Buffer {
 public:
  Buffer() = default;

  ~Buffer() {
    if (!base_address_) {
      return;
    }

    size_t mapped_bytes =
        base::bits::Align(offset_ + size_, base::GetPageSize());
    zx_status_t status = zx::vmar::root_self()->unmap(
        reinterpret_cast<uintptr_t>(base_address_), mapped_bytes);
    ZX_DCHECK(status == ZX_OK, status) << "zx_vmar_unmap";
  }

  Buffer(Buffer&&) = default;
  Buffer& operator=(Buffer&&) = default;

  bool Initialize(zx::vmo vmo,
                  size_t offset,
                  size_t size,
                  fuchsia::sysmem::CoherencyDomain coherency_domain) {
    DCHECK(!base_address_);
    DCHECK(vmo);

    // zx_vmo_write() doesn't work for sysmem-allocated VMOs (see ZX-4854), so
    // the VMOs have to be mapped.
    size_t bytes_to_map = base::bits::Align(offset + size, base::GetPageSize());
    uintptr_t addr;
    zx_status_t status = zx::vmar::root_self()->map(
        ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, /*vmar_offset=*/0, vmo,
        /*vmo_offset=*/0, bytes_to_map, &addr);
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_vmar_map";
      return false;
    }

    base_address_ = reinterpret_cast<uint8_t*>(addr);
    offset_ = offset;
    size_ = size;
    coherency_domain_ = coherency_domain;

    return true;
  }

  bool is_used() const { return is_used_; }
  size_t size() const { return size_; }

  // Copies as much data as possible from |data| to this input buffer.
  size_t Write(base::span<const uint8_t> data) {
    DCHECK(!is_used_);
    is_used_ = true;

    size_t bytes_to_fill = std::min(size_, data.size());
    memcpy(base_address_ + offset_, data.data(), bytes_to_fill);

    FlushBuffer(0, bytes_to_fill);

    return bytes_to_fill;
  }

  base::span<uint8_t> ReserveAndMapBuffer() {
    DCHECK(!is_used_);
    is_used_ = true;
    return base::make_span(base_address_ + offset_, size_);
  }

  void FlushBuffer(size_t flush_offset, size_t flush_size) {
    DCHECK(is_used_);
    DCHECK_LE(flush_size, size_ - flush_offset);

    if (coherency_domain_ != fuchsia::sysmem::CoherencyDomain::RAM)
      return;

    uint8_t* address = base_address_ + offset_ + flush_offset;
    zx_status_t status =
        zx_cache_flush(address, flush_size, ZX_CACHE_FLUSH_DATA);
    ZX_DCHECK(status == ZX_OK, status) << "zx_cache_flush";
  }

  void Release() { is_used_ = false; }

 private:
  uint8_t* base_address_ = nullptr;

  // Buffer settings provided by sysmem.
  size_t offset_ = 0;
  size_t size_ = 0;
  fuchsia::sysmem::CoherencyDomain coherency_domain_;

  // Set to true when this buffer is being used by the codec.
  bool is_used_ = false;
};

SysmemBufferWriter::SysmemBufferWriter(std::vector<Buffer> buffers)
    : buffers_(std::move(buffers)) {}

SysmemBufferWriter::~SysmemBufferWriter() = default;

size_t SysmemBufferWriter::Write(size_t index, base::span<const uint8_t> data) {
  DCHECK_LT(index, buffers_.size());
  DCHECK(!buffers_[index].is_used());

  return buffers_[index].Write(data);
}

base::Optional<size_t> SysmemBufferWriter::Acquire() {
  auto it = std::find_if(
      buffers_.begin(), buffers_.end(),
      [](const SysmemBufferWriter::Buffer& buf) { return !buf.is_used(); });

  if (it == buffers_.end())
    return base::nullopt;

  return it - buffers_.begin();
}

void SysmemBufferWriter::Release(size_t index) {
  DCHECK_LT(index, buffers_.size());
  buffers_[index].Release();
}

void SysmemBufferWriter::ReleaseAll() {
  for (auto& buf : buffers_) {
    buf.Release();
  }
}

size_t SysmemBufferWriter::num_buffers() const {
  return buffers_.size();
}

// static
std::unique_ptr<SysmemBufferWriter> SysmemBufferWriter::Create(
    fuchsia::sysmem::BufferCollectionInfo_2 info) {
  std::vector<SysmemBufferWriter::Buffer> buffers;
  buffers.resize(info.buffer_count);

  fuchsia::sysmem::BufferMemorySettings& settings =
      info.settings.buffer_settings;
  for (size_t i = 0; i < info.buffer_count; ++i) {
    fuchsia::sysmem::VmoBuffer& buffer = info.buffers[i];
    if (!buffers[i].Initialize(std::move(buffer.vmo), buffer.vmo_usable_start,
                               settings.size_bytes,
                               settings.coherency_domain)) {
      return nullptr;
    }
  }

  return std::make_unique<SysmemBufferWriter>(std::move(buffers));
}

// static
base::Optional<fuchsia::sysmem::BufferCollectionConstraints>
SysmemBufferWriter::GetRecommendedConstraints(size_t min_buffer_count,
                                              size_t min_buffer_size) {
  fuchsia::sysmem::BufferCollectionConstraints buffer_constraints;

  // Currently we have to map buffers VMOs to write to them (see ZX-4854) and
  // memory cannot be mapped as write-only (see ZX-4872), so request RW access
  // even though we will never need to read from these buffers.
  buffer_constraints.usage.cpu =
      fuchsia::sysmem::cpuUsageRead | fuchsia::sysmem::cpuUsageWrite;

  buffer_constraints.min_buffer_count = min_buffer_count;
  buffer_constraints.has_buffer_memory_constraints = true;
  buffer_constraints.buffer_memory_constraints.min_size_bytes = min_buffer_size;
  buffer_constraints.buffer_memory_constraints.ram_domain_supported = true;
  buffer_constraints.buffer_memory_constraints.cpu_domain_supported = true;

  return buffer_constraints;
}

base::span<uint8_t> SysmemBufferWriter::ReserveAndMapBuffer(size_t index) {
  DCHECK_LT(index, buffers_.size());
  return buffers_[index].ReserveAndMapBuffer();
}

void SysmemBufferWriter::FlushBuffer(size_t index,
                                     size_t flush_offset,
                                     size_t flush_size) {
  DCHECK_LT(index, buffers_.size());
  return buffers_[index].FlushBuffer(flush_offset, flush_size);
}

}  // namespace media
