// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/gpu_memory_buffer_tracker.h"

#include "base/check.h"
#include "base/notreached.h"
#include "media/capture/video/chromeos/pixel_format_utils.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "ui/gfx/geometry/size.h"

namespace media {

GpuMemoryBufferTracker::GpuMemoryBufferTracker() : buffer_(nullptr) {}

GpuMemoryBufferTracker::~GpuMemoryBufferTracker() = default;

bool GpuMemoryBufferTracker::Init(const gfx::Size& dimensions,
                                  VideoPixelFormat format,
                                  const mojom::PlaneStridesPtr& strides) {
  absl::optional<gfx::BufferFormat> gfx_format = PixFormatVideoToGfx(format);
  if (!gfx_format) {
    NOTREACHED() << "Unsupported VideoPixelFormat "
                 << VideoPixelFormatToString(format);
    return false;
  }
  // There's no consumer information here to determine the precise buffer usage,
  // so we try the usage flag that covers all use cases.
  // JPEG capture buffer is backed by R8 pixel buffer.
  const gfx::BufferUsage usage =
      *gfx_format == gfx::BufferFormat::R_8
          ? gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE
          : gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;

  buffer_ =
      buffer_factory_.CreateGpuMemoryBuffer(dimensions, *gfx_format, usage);
  if (!buffer_) {
    NOTREACHED() << "Failed to create GPU memory buffer";
    return false;
  }
  return true;
}

bool GpuMemoryBufferTracker::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
  absl::optional<gfx::BufferFormat> gfx_format = PixFormatVideoToGfx(format);
  if (!gfx_format) {
    return false;
  }
  return (*gfx_format == buffer_->GetFormat() &&
          dimensions == buffer_->GetSize());
}

std::unique_ptr<VideoCaptureBufferHandle>
GpuMemoryBufferTracker::GetMemoryMappedAccess() {
  NOTREACHED() << "Unsupported operation";
  return std::make_unique<NullHandle>();
}

base::UnsafeSharedMemoryRegion
GpuMemoryBufferTracker::DuplicateAsUnsafeRegion() {
  NOTREACHED() << "Unsupported operation";
  return base::UnsafeSharedMemoryRegion();
}

mojo::ScopedSharedBufferHandle GpuMemoryBufferTracker::DuplicateAsMojoBuffer() {
  NOTREACHED() << "Unsupported operation";
  return mojo::ScopedSharedBufferHandle();
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferTracker::GetGpuMemoryBufferHandle() {
  DCHECK(buffer_);
  // Overriding the GpuMemoryBuffer id to an invalid id to avoid buffer
  // collision in GpuMemoryBufferFactoryNativePixmap when we pass the handle
  // to a different process. (crbug.com/993265)
  //
  // This will force the GPU process to look up the real native pixmap handle
  // through the DMA-buf fds in [1] when creating SharedImage, instead of
  // re-using a wrong pixmap handle in the cache.
  //
  // [1]: https://tinyurl.com/yymtv22y
  gfx::GpuMemoryBufferHandle handle = buffer_->CloneHandle();
  handle.id = gfx::GpuMemoryBufferHandle::kInvalidId;
  return handle;
}

uint32_t GpuMemoryBufferTracker::GetMemorySizeInBytes() {
  DCHECK(buffer_);
  switch (buffer_->GetFormat()) {
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return buffer_->GetSize().width() * buffer_->GetSize().height() * 3 / 2;
    case gfx::BufferFormat::R_8:
      return buffer_->GetSize().width() * buffer_->GetSize().height();
    default:
      NOTREACHED() << "Unsupported gfx buffer format";
      return buffer_->GetSize().width() * buffer_->GetSize().height();
  }
}

}  // namespace media
