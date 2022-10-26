// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/system/sys_info.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
namespace {

// Validate that |stride| will work for pixels with |size| and |format|.
bool ValidateStride(const gfx::Size size,
                    viz::ResourceFormat format,
                    uint32_t stride) {
  if (!base::IsValueInRangeForNumericType<size_t>(stride))
    return false;

  uint32_t min_width_in_bytes = 0;
  if (!viz::ResourceSizes::MaybeWidthInBytes(size.width(), format,
                                             &min_width_in_bytes)) {
    return false;
  }

  if (stride < min_width_in_bytes)
    return false;

  // Check that stride is a multiple of pixel byte size.
  int bits_per_pixel = viz::BitsPerPixel(format);
  switch (bits_per_pixel) {
    case 64:
    case 32:
    case 16:
      if (stride % (bits_per_pixel / 8) != 0)
        return false;
      break;
    case 8:
    case 4:
      break;
    default:
      // YVU420, YUV_420_BIPLANAR, and YUVA_420_TRIPLANAR format aren't
      // supported.
      NOTREACHED();
      return false;
  }

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
    viz::ResourceFormat format) {
  DCHECK(!mapping_.IsValid());

  if (!handle.region.IsValid()) {
    DLOG(ERROR) << "Invalid GMB shared memory region.";
    return false;
  }

  if (!ValidateStride(size, format, handle.stride)) {
    DLOG(ERROR) << "Invalid GMB stride.";
    return false;
  }

  // Minimize the amount of address space we use but make sure offset is a
  // multiple of page size as required by MapAt().
  size_t allocation_granularity = base::SysInfo::VMAllocationGranularity();
  size_t memory_offset = handle.offset % allocation_granularity;
  size_t map_offset =
      allocation_granularity * (handle.offset / allocation_granularity);

  base::CheckedNumeric<size_t> checked_size = handle.stride;
  checked_size *= size.height();
  checked_size += memory_offset;
  if (!checked_size.IsValid()) {
    DLOG(ERROR) << "Invalid GMB size.";
    return false;
  }

  mapping_ = handle.region.MapAt(static_cast<off_t>(map_offset),
                                 checked_size.ValueOrDie());

  if (!mapping_.IsValid()) {
    DLOG(ERROR) << "Failed to map shared memory.";
    return false;
  }

  offset_ = memory_offset;
  stride_ = handle.stride;

  return true;
}

bool SharedMemoryRegionWrapper::IsValid() const {
  return mapping_.IsValid();
}

uint8_t* SharedMemoryRegionWrapper::GetMemory() const {
  DCHECK(IsValid());
  return mapping_.GetMemoryAs<uint8_t>() + offset_;
}

base::span<const uint8_t> SharedMemoryRegionWrapper::GetMemoryAsSpan() const {
  DCHECK(IsValid());
  return mapping_.GetMemoryAsSpan<const uint8_t>().subspan(offset_);
}

size_t SharedMemoryRegionWrapper::GetStride() const {
  DCHECK(IsValid());
  return stride_;
}

const base::UnguessableToken& SharedMemoryRegionWrapper::GetMappingGuid() {
  return mapping_.guid();
}

}  // namespace gpu
