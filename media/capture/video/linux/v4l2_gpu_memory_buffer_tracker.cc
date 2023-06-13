// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/v4l2_gpu_memory_buffer_tracker.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {
// Converts the video pixel format |pixel_format| to gfx::BufferFormat.
absl::optional<gfx::BufferFormat> ToBufferFormat(
    VideoPixelFormat pixel_format) {
  switch (pixel_format) {
    case PIXEL_FORMAT_NV12:
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    default:
      return absl::nullopt;
  }
}

gfx::BufferUsage GetBufferUsage(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
    default:
      // Default usage for YUV camera buffer.
      return gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;
  }
}
}  // namespace

V4L2GpuMemoryBufferTracker::V4L2GpuMemoryBufferTracker() = default;
V4L2GpuMemoryBufferTracker::~V4L2GpuMemoryBufferTracker() {
  if (is_valid_) {
    VideoCaptureGpuMemoryBufferManager::GetInstance().RemoveObserver(this);
  }
}

bool V4L2GpuMemoryBufferTracker::Init(const gfx::Size& dimensions,
                                      VideoPixelFormat format,
                                      const mojom::PlaneStridesPtr& strides) {
  absl::optional<gfx::BufferFormat> gfx_format = ToBufferFormat(format);
  if (!gfx_format) {
    DLOG(ERROR) << "Unsupported VideoPixelFormat "
                << VideoPixelFormatToString(format);
    return false;
  }
  gpu::GpuMemoryBufferManager* gpu_buffer_manager =
      VideoCaptureGpuMemoryBufferManager::GetInstance()
          .GetGpuMemoryBufferManager();
  if (!gpu_buffer_manager) {
    DLOG(ERROR) << "Invalid GPU memory buffer manager!";
    return false;
  }

  gfx::BufferUsage usage = GetBufferUsage(*gfx_format);
  buffer_ = gpu_buffer_manager->CreateGpuMemoryBuffer(
      dimensions, *gfx_format, usage, gpu::kNullSurfaceHandle, nullptr);
  if (!buffer_) {
    DLOG(ERROR) << "Failed to create GPU memory buffer";
    return false;
  }

  VideoCaptureGpuMemoryBufferManager::GetInstance().AddObserver(this);
  is_valid_ = true;
  return true;
}

bool V4L2GpuMemoryBufferTracker::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
  if (!is_valid_) {
    return false;
  }

  absl::optional<gfx::BufferFormat> gfx_format = ToBufferFormat(format);
  if (!gfx_format) {
    return false;
  }
  return (*gfx_format == buffer_->GetFormat() &&
          dimensions == buffer_->GetSize());
}

std::unique_ptr<VideoCaptureBufferHandle>
V4L2GpuMemoryBufferTracker::GetMemoryMappedAccess() {
  NOTREACHED_NORETURN() << "Unsupported operation";
}

base::UnsafeSharedMemoryRegion
V4L2GpuMemoryBufferTracker::DuplicateAsUnsafeRegion() {
  NOTREACHED_NORETURN() << "Unsupported operation";
}

mojo::ScopedSharedBufferHandle
V4L2GpuMemoryBufferTracker::DuplicateAsMojoBuffer() {
  NOTREACHED_NORETURN() << "Unsupported operation";
}

gfx::GpuMemoryBufferHandle
V4L2GpuMemoryBufferTracker::GetGpuMemoryBufferHandle() {
  DCHECK(buffer_);
  return buffer_->CloneHandle();
}

uint32_t V4L2GpuMemoryBufferTracker::GetMemorySizeInBytes() {
  DCHECK(buffer_);
  switch (buffer_->GetFormat()) {
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return buffer_->GetSize().width() * buffer_->GetSize().height() * 3 / 2;
    default:
      NOTREACHED_NORETURN() << "Unsupported gfx buffer format";
  }
}

void V4L2GpuMemoryBufferTracker::OnContextLost() {
  is_valid_ = false;
}

}  // namespace media
