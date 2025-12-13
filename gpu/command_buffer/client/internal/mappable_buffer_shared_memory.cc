// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/internal/mappable_buffer_shared_memory.h"

#include <stdint.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/process/memory.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

MappableBufferSharedMemory::MappableBufferSharedMemory(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::WritableSharedMemoryMapping shared_memory_mapping,
    size_t offset,
    uint32_t stride)
    : size_(size),
      format_(format),
      shared_memory_region_(std::move(shared_memory_region)),
      shared_memory_mapping_(std::move(shared_memory_mapping)),
      offset_(offset),
      stride_(stride) {}

MappableBufferSharedMemory::~MappableBufferSharedMemory() {
#if DCHECK_IS_ON()
  {
    base::AutoLock auto_lock(map_lock_);
    DCHECK_EQ(map_count_, 0u);
  }
#endif
}

void MappableBufferSharedMemory::AssertMapped() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
#endif
}

// static
std::unique_ptr<MappableBufferSharedMemory>
MappableBufferSharedMemory::CreateFromHandle(gfx::GpuMemoryBufferHandle handle,
                                             const gfx::Size& size,
                                             viz::SharedImageFormat format) {
  DCHECK(handle.region().IsValid());
  CHECK(viz::HasEquivalentBufferFormat(format));

  std::optional<size_t> minimum_stride =
      viz::SharedMemoryRowSizeForSharedImageFormat(format, 0, size.width());
  if (!minimum_stride) {
    return nullptr;
  }

  size_t min_buffer_size = 0;

  if (format.is_single_plane()) {
    if (handle.stride < minimum_stride.value()) {
      return nullptr;
    }

    base::CheckedNumeric<size_t> checked_min_buffer_size = handle.stride;
    checked_min_buffer_size *= size.height() - 1;
    checked_min_buffer_size += minimum_stride.value();
    if (!checked_min_buffer_size.AssignIfValid(&min_buffer_size)) {
      return nullptr;
    }
  } else {
    // Custom layout (i.e. non-standard stride) is not allowed for multi-plane
    // formats.
    if (handle.stride != minimum_stride.value()) {
      return nullptr;
    }

    std::optional<size_t> buffer_size =
        viz::SharedMemorySizeForSharedImageFormat(format, size);
    if (!buffer_size) {
      return nullptr;
    }
    min_buffer_size = buffer_size.value();
  }

  size_t min_buffer_size_with_offset = 0;
  if (!base::CheckAdd(handle.offset, min_buffer_size)
           .AssignIfValid(&min_buffer_size_with_offset)) {
    return nullptr;
  }

  if (min_buffer_size_with_offset > handle.region().GetSize()) {
    return nullptr;
  }

  const uint32_t offset = handle.offset;
  const uint32_t stride = handle.stride;
  return base::WrapUnique(new MappableBufferSharedMemory(
      size, format, std::move(handle).region(),
      base::WritableSharedMemoryMapping(), offset, stride));
}

std::unique_ptr<MappableBufferSharedMemory>
MappableBufferSharedMemory::CreateFromMapping(
    base::WritableSharedMemoryMapping shared_memory_mapping,
    const gfx::Size& size,
    viz::SharedImageFormat format) {
  CHECK(shared_memory_mapping.IsValid());
  std::optional<size_t> stride =
      viz::SharedMemoryRowSizeForSharedImageFormat(format, 0, size.width());
  if (!stride) {
    return nullptr;
  }

  return base::WrapUnique(new MappableBufferSharedMemory(
      size, format, base::UnsafeSharedMemoryRegion(),
      std::move(shared_memory_mapping), /*offset=*/0, stride.value()));
}

// static
base::OnceClosure MappableBufferSharedMemory::AllocateForTesting(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  auto shared_memory_region = base::UnsafeSharedMemoryRegion::Create(
      viz::SharedMemorySizeForSharedImageFormat(format, size).value());
  CHECK(shared_memory_region.IsValid());

  *handle = gfx::GpuMemoryBufferHandle(std::move(shared_memory_region));
  handle->type = gfx::SHARED_MEMORY_BUFFER;
  handle->offset = 0;
  handle->stride = static_cast<uint32_t>(
      viz::SharedMemoryRowSizeForSharedImageFormat(format, 0, size.width())
          .value());
  return base::DoNothing();
}

bool MappableBufferSharedMemory::Map() {
  base::AutoLock auto_lock(map_lock_);
  if (map_count_++) {
    DCHECK(shared_memory_mapping_.IsValid());
    return true;
  }

  // Map the buffer first time Map() is called then keep it mapped for the
  // lifetime of the buffer. This avoids mapping the buffer unless necessary.
  if (!shared_memory_mapping_.IsValid()) {
    DCHECK_EQ(
        static_cast<size_t>(stride_),
        viz::SharedMemoryRowSizeForSharedImageFormat(format_, 0, size_.width())
            .value());
    size_t buffer_size =
        viz::SharedMemorySizeForSharedImageFormat(format_, size_).value();
    // Note: offset_ != 0 is not common use-case. To keep it simple we
    // map offset + buffer_size here but this can be avoided using MapAt().
    size_t map_size = offset_ + buffer_size;
    shared_memory_mapping_ = shared_memory_region_.MapAt(0, map_size);
    if (!shared_memory_mapping_.IsValid()) {
      base::TerminateBecauseOutOfMemory(map_size);
    }
  }
  return true;
}

void* MappableBufferSharedMemory::memory(size_t plane) {
  AssertMapped();
  DCHECK_LT(static_cast<int>(plane), format_.NumberOfPlanes());
  return UNSAFE_TODO(
      static_cast<uint8_t*>(shared_memory_mapping_.memory()) + offset_ +
      viz::SharedMemoryOffsetForSharedImageFormat(format_, plane, size_));
}

void MappableBufferSharedMemory::Unmap() {
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
  --map_count_;
}

int MappableBufferSharedMemory::stride(size_t plane) const {
  DCHECK_LT(static_cast<int>(plane), format_.NumberOfPlanes());
  size_t stride = viz::SharedMemoryRowSizeForSharedImageFormat(format_, plane,
                                                               size_.width())
                      .value();
  return base::checked_cast<int>(stride);
}

gfx::GpuMemoryBufferType MappableBufferSharedMemory::GetType() const {
  return gfx::SHARED_MEMORY_BUFFER;
}

gfx::GpuMemoryBufferHandle MappableBufferSharedMemory::CloneHandle() const {
  gfx::GpuMemoryBufferHandle handle(shared_memory_region_.Duplicate());
  handle.offset = offset_;
  handle.stride = stride_;
  return handle;
}

void MappableBufferSharedMemory::MapAsync(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(Map());
}

bool MappableBufferSharedMemory::AsyncMappingIsNonBlocking() const {
  return false;
}

}  // namespace gpu
