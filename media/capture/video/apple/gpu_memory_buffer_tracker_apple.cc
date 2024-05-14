// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/gpu_memory_buffer_tracker_apple.h"

#include "base/logging.h"

namespace media {

GpuMemoryBufferTrackerApple::GpuMemoryBufferTrackerApple() {}

GpuMemoryBufferTrackerApple::GpuMemoryBufferTrackerApple(
    base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface)
    : is_external_io_surface_(true), io_surface_(std::move(io_surface)) {}

GpuMemoryBufferTrackerApple::~GpuMemoryBufferTrackerApple() {}

bool GpuMemoryBufferTrackerApple::Init(const gfx::Size& dimensions,
                                       VideoPixelFormat format,
                                       const mojom::PlaneStridesPtr& strides) {
  DCHECK(!io_surface_);
  if (format != PIXEL_FORMAT_NV12) {
    NOTREACHED_IN_MIGRATION()
        << "Unsupported VideoPixelFormat " << VideoPixelFormatToString(format);
    return false;
  }
  if ((io_surface_ =
           CreateIOSurface(dimensions, gfx::BufferFormat::YUV_420_BIPLANAR,
                           /*should_clear=*/false))) {
    DVLOG(2) << __func__ << " id " << IOSurfaceGetID(io_surface_.get());
    return true;
  } else {
    LOG(ERROR) << "Unable to create IOSurface!";
    return false;
  }
}

bool GpuMemoryBufferTrackerApple::IsSameGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle) const {
  if (!is_external_io_surface_) {
    return false;
  }
  return IOSurfaceGetID(io_surface_.get()) ==
         IOSurfaceGetID(handle.io_surface.get());
}

bool GpuMemoryBufferTrackerApple::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
  if (is_external_io_surface_) {
    return false;
  }
  gfx::Size surface_size(IOSurfaceGetWidth(io_surface_.get()),
                         IOSurfaceGetHeight(io_surface_.get()));
  return format == PIXEL_FORMAT_NV12 && dimensions == surface_size;
}

uint32_t GpuMemoryBufferTrackerApple::GetMemorySizeInBytes() {
  return IOSurfaceGetAllocSize(io_surface_.get());
}

std::unique_ptr<VideoCaptureBufferHandle>
GpuMemoryBufferTrackerApple::GetMemoryMappedAccess() {
  NOTREACHED_IN_MIGRATION() << "Unsupported operation";
  return std::make_unique<NullHandle>();
}

base::UnsafeSharedMemoryRegion
GpuMemoryBufferTrackerApple::DuplicateAsUnsafeRegion() {
  NOTREACHED_IN_MIGRATION() << "Unsupported operation";
  return base::UnsafeSharedMemoryRegion();
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferTrackerApple::GetGpuMemoryBufferHandle() {
  DVLOG(2) << __func__ << " id " << IOSurfaceGetID(io_surface_.get());
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
  gmb_handle.id = gfx::GpuMemoryBufferHandle::kInvalidId;
  gmb_handle.io_surface = io_surface_;
  return gmb_handle;
}

VideoCaptureBufferType GpuMemoryBufferTrackerApple::GetBufferType() {
  return VideoCaptureBufferType::kGpuMemoryBuffer;
}

void GpuMemoryBufferTrackerApple::OnHeldByConsumersChanged(
    bool is_held_by_consumers) {
  if (!is_external_io_surface_) {
    return;
  }

  if (is_held_by_consumers) {
    DCHECK(!in_use_for_consumers_);
    in_use_for_consumers_.reset(io_surface_.get(), base::scoped_policy::RETAIN);
  } else {
    DCHECK(in_use_for_consumers_);
    in_use_for_consumers_.reset();
  }
}

}  // namespace media
