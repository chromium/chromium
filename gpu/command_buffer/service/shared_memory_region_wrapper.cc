// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/system/sys_info.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
namespace {

// Validate that |stride| will work for pixels with |size| and |format|.
bool ValidateStride(const gfx::Size size,
                    gfx::BufferFormat format,
                    uint32_t stride) {
  if (!base::IsValueInRangeForNumericType<size_t>(stride))
    return false;

  // Use plane index 0 since we can't handle different plane strides anyway.
  size_t alignment = gfx::RowByteAlignmentForBufferFormat(format, /*plane=*/0);
  if (stride % alignment != 0)
    return false;

  size_t min_width_in_bytes = 0;
  if (!gfx::RowSizeForBufferFormatChecked(size.width(), format, /*plane=*/0,
                                          &min_width_in_bytes)) {
    return false;
  }

  if (stride < min_width_in_bytes)
    return false;

  return true;
}

}  // namespace

SharedMemoryRegionWrapper::SharedMemoryRegionWrapper() = default;
SharedMemoryRegionWrapper::SharedMemoryRegionWrapper(
    SharedMemoryRegionWrapper&& other) = default;
SharedMemoryRegionWrapper& SharedMemoryRegionWrapper::operator=(
    SharedMemoryRegionWrapper&& other) = default;
SharedMemoryRegionWrapper::~SharedMemoryRegionWrapper() = default;

bool SharedMemoryRegionWrapper::Initialize(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferPlane plane) {
  DCHECK(!mapping_.IsValid());

  if (!handle.region.IsValid()) {
    DLOG(ERROR) << "Invalid GMB shared memory region.";
    return false;
  }

  if (!ValidateStride(size, format, handle.stride)) {
    DLOG(ERROR) << "Invalid GMB stride.";
    return false;
  }

  size_t buffer_size;
  if (!gfx::BufferSizeForBufferFormatChecked(size, format, &buffer_size)) {
    DLOG(ERROR) << "Invalid GMB size.";
    return false;
  }

  // Minimize the amount of address space we use but make sure offset is a
  // multiple of page size as required by MapAt().
  // TODO(sunnyps): This doesn't seem to be a requirement of MapAt() anymore.
  const size_t allocation_granularity =
      base::SysInfo::VMAllocationGranularity();
  const size_t memory_offset = handle.offset % allocation_granularity;
  const size_t map_offset =
      allocation_granularity * (handle.offset / allocation_granularity);

  base::CheckedNumeric<size_t> checked_size = buffer_size;
  checked_size += memory_offset;
  if (!checked_size.IsValid()) {
    DLOG(ERROR) << "Invalid shared memory region map size.";
    return false;
  }

  const size_t map_size = checked_size.ValueOrDie();
  mapping_ = handle.region.MapAt(static_cast<off_t>(map_offset), map_size);
  if (!mapping_.IsValid()) {
    DLOG(ERROR) << "Failed to map shared memory.";
    return false;
  }

  size_t num_planes = gfx::NumberOfPlanesForLinearBufferFormat(format);
  planes_.resize(num_planes);

  if (num_planes > 1 && plane == gfx::BufferPlane::DEFAULT) {
    // The offset/stride only make sense when GpuMemoryBufferHandle is for a
    // single plane. Stride should be set as the expected stride for first plane
    // and offset should always be zero.
    DCHECK_EQ(static_cast<size_t>(handle.stride),
              gfx::RowSizeForBufferFormat(size.width(), format, /*plane=*/0));
    DCHECK_EQ(handle.offset, 0u);

    for (size_t plane_index = 0; plane_index < num_planes; ++plane_index) {
      const size_t plane_offset =
          gfx::BufferOffsetForBufferFormat(size, format, plane_index);

      planes_[plane_index].offset = memory_offset + plane_offset;
      planes_[plane_index].stride =
          RowSizeForBufferFormat(size.width(), format, plane_index);
    }
  } else {
    // Add plane offset separately so that we map the entire buffer even if
    // we're accessing an individual plane - this helps with shared memory
    // overlays on Windows by allowing access via the Y plane shared image only.
    const size_t plane_index = GetPlaneIndex(plane, format);
    const size_t plane_offset =
        gfx::BufferOffsetForBufferFormat(size, format, plane_index);
#if DCHECK_IS_ON()
    const gfx::Size plane_size = GetPlaneSize(plane, size);
    const size_t plane_size_bytes =
        gfx::PlaneSizeForBufferFormat(plane_size, format, plane_index);
    DCHECK_LE(memory_offset + plane_offset + plane_size_bytes, map_size);
#endif

    planes_[0].offset = memory_offset + plane_offset;
    planes_[0].stride = handle.stride;
  }

  return true;
}

bool SharedMemoryRegionWrapper::IsValid() const {
  return mapping_.IsValid();
}

uint8_t* SharedMemoryRegionWrapper::GetMemory(int plane_index) const {
  DCHECK(IsValid());
  return mapping_.GetMemoryAs<uint8_t>() + planes_[plane_index].offset;
}

size_t SharedMemoryRegionWrapper::GetStride(int plane_index) const {
  DCHECK(IsValid());
  return planes_[plane_index].stride;
}

SkPixmap SharedMemoryRegionWrapper::MakePixmapForPlane(const SkImageInfo& info,
                                                       int plane_index) const {
  DCHECK(IsValid());

  SkPixmap pixmap(info, GetMemory(plane_index), GetStride(plane_index));
  DCHECK_LE(planes_[plane_index].offset + pixmap.computeByteSize(),
            mapping_.mapped_size());
  return pixmap;
}

const base::UnguessableToken& SharedMemoryRegionWrapper::GetMappingGuid()
    const {
  return mapping_.guid();
}

}  // namespace gpu
