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
  std::optional<viz::SharedImageFormat> si_format =
      VideoPixelFormatToVizSIFormat(format);
  if (!si_format) {
    NOTREACHED() << "Unsupported VideoPixelFormat "
                 << VideoPixelFormatToString(format);
  }
  // There's no consumer information here to determine the precise buffer usage,
  // so we try the usage flag that covers all use cases.
  // JPEG capture buffer is backed by R8 pixel buffer.
  const gfx::BufferUsage usage =
      format == PIXEL_FORMAT_MJPEG
          ? gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE
          : gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;
  shared_image_ =
      buffer_factory_.CreateSharedImage(dimensions, *si_format, usage);
  return !!shared_image_;
}

bool GpuMemoryBufferTrackerCros::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
  std::optional<viz::SharedImageFormat> si_format =
      VideoPixelFormatToVizSIFormat(format);
  if (!si_format) {
    return false;
  }
  return (*si_format == shared_image_->format() &&
          dimensions == shared_image_->size());
}

std::unique_ptr<VideoCaptureBufferHandle>
GpuMemoryBufferTrackerCros::GetMemoryMappedAccess() {
  NOTREACHED() << "Unsupported operation";
}

base::UnsafeSharedMemoryRegion
GpuMemoryBufferTrackerCros::DuplicateAsUnsafeRegion() {
  NOTREACHED() << "Unsupported operation";
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferTrackerCros::GetGpuMemoryBufferHandle() {
  CHECK(shared_image_);

  // TODO(crbug.com/359601431): Change this flow to talk entirely in terms of
  // SharedImage once all GpuMemoryBufferTrackers are converted to use
  // MappableSI.
  gfx::GpuMemoryBufferHandle handle =
      shared_image_->CloneGpuMemoryBufferHandle();
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
    NOTREACHED() << "Unsupported shared image format";
  }
  return size;
}

}  // namespace media
