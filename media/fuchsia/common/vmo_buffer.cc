// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/fuchsia/common/vmo_buffer.h"

#include <zircon/rights.h>
#include <algorithm>

#include "base/bits.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/memory/page_size.h"

namespace media {

// static
fuchsia::sysmem2::BufferCollectionConstraints
VmoBuffer::GetRecommendedConstraints(size_t min_buffer_count,
                                     std::optional<size_t> min_buffer_size,
                                     bool writable) {
  fuchsia::sysmem2::BufferCollectionConstraints constraints;

  constraints.mutable_usage()->set_cpu(fuchsia::sysmem2::CPU_USAGE_READ);
  if (writable) {
    *constraints.mutable_usage()->mutable_cpu() |=
        fuchsia::sysmem2::CPU_USAGE_WRITE;
  }

  constraints.set_min_buffer_count(min_buffer_count);

  if (min_buffer_size.has_value()) {
    auto& memory_constraints = *constraints.mutable_buffer_memory_constraints();
    memory_constraints.set_min_size_bytes(min_buffer_size.value());
    memory_constraints.set_ram_domain_supported(true);
    memory_constraints.set_cpu_domain_supported(true);
  }

  return constraints;
}

// static
std::vector<VmoBuffer> VmoBuffer::CreateBuffersFromSysmemCollection(
    fuchsia::sysmem2::BufferCollectionInfo* info,
    bool writable) {
  std::vector<VmoBuffer> buffers;
  buffers.resize(info->buffers().size());

  const fuchsia::sysmem2::BufferMemorySettings& settings =
      info->settings().buffer_settings();
  for (size_t i = 0; i < info->buffers().size(); ++i) {
    fuchsia::sysmem2::VmoBuffer& buffer = info->mutable_buffers()->at(i);
    if (!buffers[i].Initialize(std::move(*buffer.mutable_vmo()), writable,
                               buffer.vmo_usable_start(), settings.size_bytes(),
                               settings.coherency_domain())) {
      return {};
    }
  }

  return buffers;
}

VmoBuffer::VmoBuffer() = default;

VmoBuffer::~VmoBuffer() {
  if (!base_address_) {
    return;
  }

  zx_status_t status = zx::vmar::root_self()->unmap(
      reinterpret_cast<uintptr_t>(base_address_), mapped_size());
  ZX_DCHECK(status == ZX_OK, status) << "zx_vmar_unmap";
}

VmoBuffer::VmoBuffer(VmoBuffer&&) = default;
VmoBuffer& VmoBuffer::operator=(VmoBuffer&&) = default;

bool VmoBuffer::Initialize(zx::vmo vmo,
                           bool writable,
                           size_t offset,
                           size_t size,
                           fuchsia::sysmem2::CoherencyDomain coherency_domain) {
  DCHECK(!base_address_);
  DCHECK(vmo);

  writable_ = writable;
  offset_ = offset;
  size_ = size;
  coherency_domain_ = coherency_domain;

  zx_vm_option_t options = ZX_VM_PERM_READ;
  if (writable)
    options |= ZX_VM_PERM_WRITE;

  uintptr_t addr;
  zx_status_t status =
      zx::vmar::root_self()->map(options, /*vmar_offset=*/0, vmo,
                                 /*vmo_offset=*/0, mapped_size(), &addr);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmar_map";
    return false;
  }

  vmo_ = std::move(vmo);
  base_address_ = reinterpret_cast<uint8_t*>(addr);

  return true;
}

size_t VmoBuffer::Read(size_t offset, base::span<uint8_t> data) {
  if (offset >= size_)
    return 0U;
  size_t bytes_to_fill = std::min(size_ - offset, data.size());
  FlushCache(offset, bytes_to_fill, /*invalidate=*/true);
  memcpy(data.data(), base_address_ + offset_ + offset, bytes_to_fill);
  return bytes_to_fill;
}

size_t VmoBuffer::Write(base::span<const uint8_t> data) {
  DCHECK(writable_);

  size_t bytes_to_fill = std::min(size_, data.size());
  memcpy(base_address_ + offset_, data.data(), bytes_to_fill);

  FlushCache(0, bytes_to_fill, /*invalidate=*/false);

  return bytes_to_fill;
}

base::span<const uint8_t> VmoBuffer::GetMemory() {
  FlushCache(0, size_, /*invalidate=*/true);
  return base::make_span(base_address_ + offset_, size_);
}

base::span<uint8_t> VmoBuffer::GetWritableMemory() {
  DCHECK(writable_);
  return base::make_span(base_address_ + offset_, size_);
}

void VmoBuffer::FlushCache(size_t flush_offset,
                           size_t flush_size,
                           bool invalidate) {
  DCHECK_LE(flush_size, size_ - flush_offset);

  if (coherency_domain_ != fuchsia::sysmem2::CoherencyDomain::RAM) {
    return;
  }

  uint8_t* address = base_address_ + offset_ + flush_offset;
  uint32_t options = ZX_CACHE_FLUSH_DATA;
  if (invalidate)
    options |= ZX_CACHE_FLUSH_INVALIDATE;
  zx_status_t status = zx_cache_flush(address, flush_size, options);
  ZX_DCHECK(status == ZX_OK, status) << "zx_cache_flush";
}

size_t VmoBuffer::mapped_size() {
  return base::bits::AlignUp(offset_ + size_, base::GetPageSize());
}

zx::vmo VmoBuffer::Duplicate(bool writable) {
  zx_rights_t rights = ZX_RIGHT_DUPLICATE | ZX_RIGHT_TRANSFER | ZX_RIGHT_READ |
                       ZX_RIGHT_MAP | ZX_RIGHT_GET_PROPERTY;
  if (writable)
    rights |= ZX_RIGHT_WRITE;

  zx::vmo vmo;
  zx_status_t status = vmo_.duplicate(rights, &vmo);
  ZX_CHECK(status == ZX_OK, status) << "zx_handle_duplicate";

  return vmo;
}

}  // namespace media
