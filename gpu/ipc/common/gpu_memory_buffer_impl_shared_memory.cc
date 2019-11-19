// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/process/memory.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/process_memory_dump.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

GpuMemoryBufferImplSharedMemory::GpuMemoryBufferImplSharedMemory(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    DestructionCallback callback,
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::WritableSharedMemoryMapping shared_memory_mapping,
    size_t offset,
    int stride)
    : GpuMemoryBufferImpl(id, size, format, std::move(callback)),
      shared_memory_region_(std::move(shared_memory_region)),
      shared_memory_mapping_(std::move(shared_memory_mapping)),
      offset_(offset),
      stride_(stride) {
  DCHECK(IsUsageSupported(usage));
  DCHECK(IsSizeValidForFormat(size, format));
}

GpuMemoryBufferImplSharedMemory::~GpuMemoryBufferImplSharedMemory() = default;

// static
std::unique_ptr<GpuMemoryBufferImplSharedMemory>
GpuMemoryBufferImplSharedMemory::Create(gfx::GpuMemoryBufferId id,
                                        const gfx::Size& size,
                                        gfx::BufferFormat format,
                                        gfx::BufferUsage usage,
                                        DestructionCallback callback) {
  if (!IsUsageSupported(usage))
    return nullptr;
  size_t buffer_size = 0u;
  if (!gfx::BufferSizeForBufferFormatChecked(size, format, &buffer_size))
    return nullptr;

  auto shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  auto shared_memory_mapping = shared_memory_region.Map();
  if (!shared_memory_region.IsValid() || !shared_memory_mapping.IsValid())
    return nullptr;

  return base::WrapUnique(new GpuMemoryBufferImplSharedMemory(
      id, size, format, usage, std::move(callback),
      std::move(shared_memory_region), std::move(shared_memory_mapping), 0,
      gfx::RowSizeForBufferFormat(size.width(), format, 0)));
}

// static
gfx::GpuMemoryBufferHandle
GpuMemoryBufferImplSharedMemory::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  if (!IsUsageSupported(usage))
    return gfx::GpuMemoryBufferHandle();
  size_t buffer_size = 0u;
  if (!gfx::BufferSizeForBufferFormatChecked(size, format, &buffer_size))
    return gfx::GpuMemoryBufferHandle();

  auto shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  if (!shared_memory_region.IsValid())
    return gfx::GpuMemoryBufferHandle();

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = id;
  handle.offset = 0;
  handle.stride = static_cast<int32_t>(
      gfx::RowSizeForBufferFormat(size.width(), format, 0));
  handle.region = std::move(shared_memory_region);
  return handle;
}

// static
std::unique_ptr<GpuMemoryBufferImplSharedMemory>
GpuMemoryBufferImplSharedMemory::CreateFromHandle(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    DestructionCallback callback) {
  DCHECK(handle.region.IsValid());

  size_t minimum_stride = 0;
  if (!gfx::RowSizeForBufferFormatChecked(size.width(), format, 0,
                                          &minimum_stride)) {
    return nullptr;
  }

  size_t min_buffer_size = 0;

  if (gfx::NumberOfPlanesForLinearBufferFormat(format) == 1) {
    if (static_cast<size_t>(handle.stride) < minimum_stride)
      return nullptr;

    base::CheckedNumeric<size_t> checked_min_buffer_size =
        base::MakeCheckedNum(handle.stride) *
            (base::MakeCheckedNum(size.height()) - 1) +
        minimum_stride;
    if (!checked_min_buffer_size.AssignIfValid(&min_buffer_size))
      return nullptr;
  } else {
    // Custom layout (i.e. non-standard stride) is not allowed for multi-plane
    // formats.
    if (static_cast<size_t>(handle.stride) != minimum_stride)
      return nullptr;

    if (!gfx::BufferSizeForBufferFormatChecked(size, format,
                                               &min_buffer_size)) {
      return nullptr;
    }
  }

  size_t min_buffer_size_with_offset = 0;
  if (!base::CheckAdd(handle.offset, min_buffer_size)
           .AssignIfValid(&min_buffer_size_with_offset)) {
    return nullptr;
  }

  if (min_buffer_size_with_offset > handle.region.GetSize()) {
    return nullptr;
  }

  return base::WrapUnique(new GpuMemoryBufferImplSharedMemory(
      handle.id, size, format, usage, std::move(callback),
      std::move(handle.region), base::WritableSharedMemoryMapping(),
      handle.offset, handle.stride));
}

// static
bool GpuMemoryBufferImplSharedMemory::IsUsageSupported(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      return true;
    case gfx::BufferUsage::SCANOUT:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return false;
  }
  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImplSharedMemory::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return IsUsageSupported(usage);
}

// static
bool GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(
    const gfx::Size& size,
    gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::RGBA_F16:
      return true;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::P010: {
      size_t num_planes = gfx::NumberOfPlanesForLinearBufferFormat(format);
      for (size_t i = 0; i < num_planes; ++i) {
        size_t factor = gfx::SubsamplingFactorForBufferFormat(format, i);
        if (size.width() % factor || size.height() % factor)
          return false;
      }
      return true;
    }
  }

  NOTREACHED();
  return false;
}

// static
base::OnceClosure GpuMemoryBufferImplSharedMemory::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  *handle = CreateGpuMemoryBuffer(handle->id, size, format, usage);
  return base::DoNothing();
}

bool GpuMemoryBufferImplSharedMemory::Map() {
  DCHECK(!mapped_);

  // Map the buffer first time Map() is called then keep it mapped for the
  // lifetime of the buffer. This avoids mapping the buffer unless necessary.
  if (!shared_memory_mapping_.IsValid()) {
    DCHECK_EQ(static_cast<size_t>(stride_),
              gfx::RowSizeForBufferFormat(size_.width(), format_, 0));
    size_t buffer_size = gfx::BufferSizeForBufferFormat(size_, format_);
    // Note: offset_ != 0 is not common use-case. To keep it simple we
    // map offset + buffer_size here but this can be avoided using MapAt().
    size_t map_size = offset_ + buffer_size;
    shared_memory_mapping_ = shared_memory_region_.MapAt(0, map_size);
    if (!shared_memory_mapping_.IsValid())
      base::TerminateBecauseOutOfMemory(map_size);
  }
  mapped_ = true;
  return true;
}

void* GpuMemoryBufferImplSharedMemory::memory(size_t plane) {
  DCHECK(mapped_);
  DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
  return static_cast<uint8_t*>(shared_memory_mapping_.memory()) + offset_ +
         gfx::BufferOffsetForBufferFormat(size_, format_, plane);
}

void GpuMemoryBufferImplSharedMemory::Unmap() {
  DCHECK(mapped_);
  mapped_ = false;
}

int GpuMemoryBufferImplSharedMemory::stride(size_t plane) const {
  DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
  return gfx::RowSizeForBufferFormat(size_.width(), format_, plane);
}

gfx::GpuMemoryBufferType GpuMemoryBufferImplSharedMemory::GetType() const {
  return gfx::SHARED_MEMORY_BUFFER;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplSharedMemory::CloneHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = id_;
  handle.offset = offset_;
  handle.stride = stride_;
  handle.region = shared_memory_region_.Duplicate();
  return handle;
}

void GpuMemoryBufferImplSharedMemory::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
    uint64_t tracing_process_id,
    int importance) const {
  pmd->CreateSharedMemoryOwnershipEdge(buffer_dump_guid, GetSharedMemoryGUID(),
                                       importance);
}

base::UnguessableToken GpuMemoryBufferImplSharedMemory::GetSharedMemoryGUID()
    const {
  return shared_memory_region_.GetGUID();
}

}  // namespace gpu
