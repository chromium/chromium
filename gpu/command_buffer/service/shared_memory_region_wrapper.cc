// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/system/sys_info.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gpu {
namespace {

size_t RowByteAlignmentForSharedImageFormatFirstPlane(
    viz::SharedImageFormat format) {
  CHECK(viz::HasEquivalentBufferFormat(format));
  if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return 8;
  }
  if (format.is_single_plane()) {
    return 4;
  }
  return format.MultiplanarStorageBytesPerChannel();
}

// Validate that |stride| will work for pixels with |size| and |format|.
bool ValidateStride(const gfx::Size size,
                    viz::SharedImageFormat format,
                    uint32_t stride) {
  if (!base::IsValueInRangeForNumericType<size_t>(stride))
    return false;

  // Check for first plane index since we can't handle different plane strides
  // anyway.
  size_t alignment = RowByteAlignmentForSharedImageFormatFirstPlane(format);
  if (stride % alignment != 0)
    return false;

  std::optional<size_t> min_width_in_bytes =
      viz::SharedMemoryRowSizeForSharedImageFormat(format, /*plane=*/0,
                                                   size.width());
  if (!min_width_in_bytes) {
    return false;
  }

  if (stride < min_width_in_bytes.value()) {
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
    viz::SharedImageFormat format) {
  DCHECK(!mapping_.IsValid());
  CHECK(viz::HasEquivalentBufferFormat(format));

  if (!handle.region().IsValid()) {
    DLOG(ERROR) << "Invalid GMB shared memory region.";
    return false;
  }

  if (!ValidateStride(size, format, handle.stride)) {
    DLOG(ERROR) << "Invalid GMB stride.";
    return false;
  }

  std::optional<size_t> buffer_size =
      viz::SharedMemorySizeForSharedImageFormat(format, size);
  if (!buffer_size) {
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

  base::CheckedNumeric<size_t> checked_size = buffer_size.value();
  checked_size += memory_offset;
  if (!checked_size.IsValid()) {
    DLOG(ERROR) << "Invalid shared memory region map size.";
    return false;
  }

  const size_t map_size = checked_size.ValueOrDie();
  mapping_ = handle.region().MapAt(static_cast<off_t>(map_offset), map_size);
  if (!mapping_.IsValid()) {
    DLOG(ERROR) << "Failed to map shared memory.";
    return false;
  }

  int num_planes = format.NumberOfPlanes();
  planes_.resize(num_planes);

  // The offset/stride only make sense when GpuMemoryBufferHandle is for a
  // single plane. Stride should be set as the expected stride for first plane
  // and offset should always be zero.
  DCHECK_EQ(static_cast<size_t>(handle.stride),
            viz::SharedMemoryRowSizeForSharedImageFormat(format, /*plane=*/0,
                                                         size.width())
                .value());
  DCHECK_EQ(handle.offset, 0u);

  for (int plane_index = 0; plane_index < num_planes; ++plane_index) {
    const size_t plane_offset =
        viz::SharedMemoryOffsetForSharedImageFormat(format, plane_index, size);

    planes_[plane_index].offset = memory_offset + plane_offset;
    planes_[plane_index].stride = viz::SharedMemoryRowSizeForSharedImageFormat(
                                      format, plane_index, size.width())
                                      .value();
  }

  return true;
}

bool SharedMemoryRegionWrapper::IsValid() const {
  return mapping_.IsValid();
}

const uint8_t* SharedMemoryRegionWrapper::GetMemory(int plane_index) const {
  DCHECK(IsValid());
  return UNSAFE_TODO(mapping_.GetMemoryAs<const uint8_t>() +
                     planes_[plane_index].offset);
}

size_t SharedMemoryRegionWrapper::GetStride(int plane_index) const {
  DCHECK(IsValid());
  return planes_[plane_index].stride;
}

base::span<const uint8_t> SharedMemoryRegionWrapper::GetMemoryPlanes() const {
  DCHECK(IsValid());
  auto full_mapped_span = UNSAFE_TODO(base::span(
      mapping_.GetMemoryAs<const uint8_t>(), mapping_.mapped_size()));
  // It is possible that the first plane starts at a non-zero offset. So we
  // subspan at this offset.
  return full_mapped_span.subspan(planes_[0].offset);
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
