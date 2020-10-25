// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/gpu_memory_buffer_tracker_mac.h"

#include "base/logging.h"

namespace media {

GpuMemoryBufferTrackerMac::GpuMemoryBufferTrackerMac() {}

GpuMemoryBufferTrackerMac::~GpuMemoryBufferTrackerMac() {}

bool GpuMemoryBufferTrackerMac::Init(const gfx::Size& dimensions,
                                     VideoPixelFormat format,
                                     const mojom::PlaneStridesPtr& strides) {
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

bool GpuMemoryBufferTrackerMac::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
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
  gmb_handle.id.id = -1;
  gmb_handle.io_surface = io_surface_;
  return gmb_handle;
}

}  // namespace media
