// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/gpu_memory_buffer_tracker_cros.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "media/capture/video/chromeos/pixel_format_utils.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "ui/gfx/geometry/size.h"

namespace media {

GpuMemoryBufferTrackerCros::GpuMemoryBufferTrackerCros() = default;

GpuMemoryBufferTrackerCros::~GpuMemoryBufferTrackerCros() = default;

bool GpuMemoryBufferTrackerCros::Init(const gfx::Size& dimensions,
                                      VideoPixelFormat format,
                                      const mojom::PlaneStridesPtr& strides) {
  std::optional<gfx::BufferFormat> gfx_format = PixFormatVideoToGfx(format);
  if (!gfx_format) {
    NOTREACHED_IN_MIGRATION()
        << "Unsupported VideoPixelFormat " << VideoPixelFormatToString(format);
    return false;
  }
  // There's no consumer information here to determine the precise buffer usage,
  // so we try the usage flag that covers all use cases.
  // JPEG capture buffer is backed by R8 pixel buffer.
  const gfx::BufferUsage usage =
      *gfx_format == gfx::BufferFormat::R_8
          ? gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE
          : gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;

  shared_image_ =
      buffer_factory_.CreateSharedImage(dimensions, *gfx_format, usage);
  return shared_image_ ? true : false;
}

bool GpuMemoryBufferTrackerCros::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
  std::optional<gfx::BufferFormat> gfx_format = PixFormatVideoToGfx(format);
  if (!gfx_format) {
    return false;
  }
  return (viz::GetSharedImageFormat(*gfx_format) == shared_image_->format() &&
          dimensions == shared_image_->size());
}

std::unique_ptr<VideoCaptureBufferHandle>
GpuMemoryBufferTrackerCros::GetMemoryMappedAccess() {
  NOTREACHED_IN_MIGRATION() << "Unsupported operation";
  return std::make_unique<NullHandle>();
}

base::UnsafeSharedMemoryRegion
GpuMemoryBufferTrackerCros::DuplicateAsUnsafeRegion() {
  NOTREACHED_IN_MIGRATION() << "Unsupported operation";
  return base::UnsafeSharedMemoryRegion();
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferTrackerCros::GetGpuMemoryBufferHandle() {
  CHECK(shared_image_);
  // Overriding the GpuMemoryBufferHandle id to an invalid id to avoid buffer
  // collision in GpuMemoryBufferFactoryNativePixmap when we pass the handle
  // to a different process. (crbug.com/993265)
  //
  // This will force the GPU process to look up the real native pixmap handle
  // through the DMA-buf fds in [1] when creating SharedImage, instead of
  // re-using a wrong pixmap handle in the cache.
  //
  // [1]: https://tinyurl.com/yymtv22y
  // TODO(crbug.com/359601431): Remove this method once all
  // GpuMemoryBufferTrackers are converted to use MappableSI.
  // Note that the above case of buffer collision will not be an issue with use
  // of MappableSI everywhere since it does not internally use or cache buffer
  // ids to refer to underlying buffer. Instead all the shared images are
  // referred to by mailboxes.
  gfx::GpuMemoryBufferHandle handle =
      shared_image_->CloneGpuMemoryBufferHandle();
  handle.id = gfx::GpuMemoryBufferHandle::kInvalidId;
  return handle;
}

VideoCaptureBufferType GpuMemoryBufferTrackerCros::GetBufferType() {
  return VideoCaptureBufferType::kGpuMemoryBuffer;
}

uint32_t GpuMemoryBufferTrackerCros::GetMemorySizeInBytes() {
  CHECK(shared_image_);
  auto size =
      shared_image_->format().EstimatedSizeInBytes(shared_image_->size());
  if ((shared_image_->format() != viz::MultiPlaneFormat::kNV12) &&
      (shared_image_->format() != viz::SinglePlaneFormat::kR_8)) {
    NOTREACHED_IN_MIGRATION() << "Unsupported shared image format";
  }
  return size;
}

}  // namespace media
