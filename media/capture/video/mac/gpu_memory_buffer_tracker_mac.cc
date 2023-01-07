// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/gpu_memory_buffer_tracker_mac.h"

#include "base/logging.h"

namespace media {

GpuMemoryBufferTrackerMac::GpuMemoryBufferTrackerMac() {}

GpuMemoryBufferTrackerMac::GpuMemoryBufferTrackerMac(
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface)
    : is_external_io_surface_(true), io_surface_(std::move(io_surface)) {}

GpuMemoryBufferTrackerMac::~GpuMemoryBufferTrackerMac() {}

bool GpuMemoryBufferTrackerMac::Init(const gfx::Size& dimensions,
                                     VideoPixelFormat format,
                                     const mojom::PlaneStridesPtr& strides) {
  DCHECK(!io_surface_);
  if (format != PIXEL_FORMAT_NV12) {
    NOTREACHED() << "Unsupported VideoPixelFormat "
                 << VideoPixelFormatToString(format);
    return false;
  }
  if (IOSurfaceRef io_surface =
          CreateIOSurface(dimensions, gfx::BufferFormat::YUV_420_BIPLANAR,
                          /*should_clear=*/false)) {
    io_surface_.reset(io_surface, base::scoped_policy::ASSUME);
    DVLOG(2) << __func__ << " id " << IOSurfaceGetID(io_surface_);
    return true;
  } else {
    LOG(ERROR) << "Unable to create IOSurface!";
    return false;
  }
}

bool GpuMemoryBufferTrackerMac::IsSameGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle) const {
  if (!is_external_io_surface_)
    return false;
  return IOSurfaceGetID(io_surface_) == IOSurfaceGetID(handle.io_surface);
}

bool GpuMemoryBufferTrackerMac::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
  if (is_external_io_surface_)
    return false;
  gfx::Size surface_size(IOSurfaceGetWidth(io_surface_),
                         IOSurfaceGetHeight(io_surface_));
  return format == PIXEL_FORMAT_NV12 && dimensions == surface_size;
}

uint32_t GpuMemoryBufferTrackerMac::GetMemorySizeInBytes() {
  return IOSurfaceGetAllocSize(io_surface_);
}

std::unique_ptr<VideoCaptureBufferHandle>
GpuMemoryBufferTrackerMac::GetMemoryMappedAccess() {
  NOTREACHED() << "Unsupported operation";
  return std::make_unique<NullHandle>();
}

base::UnsafeSharedMemoryRegion
GpuMemoryBufferTrackerMac::DuplicateAsUnsafeRegion() {
  NOTREACHED() << "Unsupported operation";
  return base::UnsafeSharedMemoryRegion();
}

mojo::ScopedSharedBufferHandle
GpuMemoryBufferTrackerMac::DuplicateAsMojoBuffer() {
  NOTREACHED() << "Unsupported operation";
  return mojo::ScopedSharedBufferHandle();
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferTrackerMac::GetGpuMemoryBufferHandle() {
  DVLOG(2) << __func__ << " id " << IOSurfaceGetID(io_surface_);
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
  gmb_handle.id = gfx::GpuMemoryBufferHandle::kInvalidId;
  gmb_handle.io_surface = io_surface_;
  return gmb_handle;
}

void GpuMemoryBufferTrackerMac::OnHeldByConsumersChanged(
    bool is_held_by_consumers) {
  if (!is_external_io_surface_)
    return;

  if (is_held_by_consumers) {
    DCHECK(!in_use_for_consumers_);
    in_use_for_consumers_.reset(io_surface_.get(), base::scoped_policy::RETAIN);
  } else {
    DCHECK(in_use_for_consumers_);
    in_use_for_consumers_.reset();
  }
}

}  // namespace media
