// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/v4l2_gpu_memory_buffer_tracker.h"

#include <optional>

#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {
// Converts the video pixel format |pixel_format| to viz::SharedImageFormat.
std::optional<viz::SharedImageFormat> ToSharedImageFormat(
    VideoPixelFormat pixel_format) {
  switch (pixel_format) {
    case PIXEL_FORMAT_NV12:
      return viz::MultiPlaneFormat::kNV12;
    default:
      return std::nullopt;
  }
}

gfx::BufferUsage GetBufferUsage(viz::SharedImageFormat format) {
  if (format == viz::MultiPlaneFormat::kNV12) {
    return gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
  }
  // Default usage for YUV camera buffer.
  return gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;
}
}  // namespace

V4L2GpuMemoryBufferTracker::V4L2GpuMemoryBufferTracker() = default;
V4L2GpuMemoryBufferTracker::~V4L2GpuMemoryBufferTracker() {
  VideoCaptureGpuChannelHost::GetInstance().RemoveObserver(this);
}

bool V4L2GpuMemoryBufferTracker::Init(const gfx::Size& dimensions,
                                      VideoPixelFormat format,
                                      const mojom::PlaneStridesPtr& strides) {
  std::optional<viz::SharedImageFormat> si_format = ToSharedImageFormat(format);
  if (!si_format) {
    DLOG(ERROR) << "Unsupported VideoPixelFormat "
                << VideoPixelFormatToString(format);
    return false;
  }
  gfx::BufferUsage usage = GetBufferUsage(*si_format);

  auto sii =
      VideoCaptureGpuChannelHost::GetInstance().GetSharedImageInterface();
  if (!sii) {
    LOG(ERROR) << "Failed to get SharedImageInterface.";
    return false;
  }

  // Setting some default usage in order to get a mappable shared image.
  const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  shared_image_ = sii->CreateSharedImage(
      {*si_format, dimensions, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(si_usage), "V4L2GpuMemoryBufferTracker"},
      gpu::kNullSurfaceHandle, usage);
  if (!shared_image_) {
    LOG(ERROR) << "Failed to create a mappable shared image.";
    return false;
  }

  VideoCaptureGpuChannelHost::GetInstance().AddObserver(this);
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

  std::optional<viz::SharedImageFormat> si_format = ToSharedImageFormat(format);
  if (!si_format) {
    return false;
  }
  return (*si_format == shared_image_->format() &&
          dimensions == shared_image_->size());
}

std::unique_ptr<VideoCaptureBufferHandle>
V4L2GpuMemoryBufferTracker::GetMemoryMappedAccess() {
  NOTREACHED() << "Unsupported operation";
}

base::UnsafeSharedMemoryRegion
V4L2GpuMemoryBufferTracker::DuplicateAsUnsafeRegion() {
  NOTREACHED() << "Unsupported operation";
}

gfx::GpuMemoryBufferHandle
V4L2GpuMemoryBufferTracker::GetGpuMemoryBufferHandle() {
  return shared_image_->CloneGpuMemoryBufferHandle();
}

VideoCaptureBufferType V4L2GpuMemoryBufferTracker::GetBufferType() {
  return VideoCaptureBufferType::kGpuMemoryBuffer;
}

uint32_t V4L2GpuMemoryBufferTracker::GetMemorySizeInBytes() {
  if (shared_image_->format() == viz::MultiPlaneFormat::kNV12) {
    return shared_image_->format().EstimatedSizeInBytes(shared_image_->size());
  }
  NOTREACHED() << "Unsupported shared image format";
}

void V4L2GpuMemoryBufferTracker::OnContextLost() {
  is_valid_ = false;
}

}  // namespace media
