// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/v4l2_gpu_memory_buffer_tracker.h"

#include <optional>

#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {
// Converts the video pixel format |pixel_format| to gfx::BufferFormat.
std::optional<gfx::BufferFormat> ToBufferFormat(VideoPixelFormat pixel_format) {
  switch (pixel_format) {
    case PIXEL_FORMAT_NV12:
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    default:
      return std::nullopt;
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
    VideoCaptureGpuChannelHost::GetInstance().RemoveObserver(this);
  }
}

bool V4L2GpuMemoryBufferTracker::Init(const gfx::Size& dimensions,
                                      VideoPixelFormat format,
                                      const mojom::PlaneStridesPtr& strides) {
  std::optional<gfx::BufferFormat> gfx_format = ToBufferFormat(format);
  if (!gfx_format) {
    DLOG(ERROR) << "Unsupported VideoPixelFormat "
                << VideoPixelFormatToString(format);
    return false;
  }
  gfx::BufferUsage usage = GetBufferUsage(*gfx_format);

  auto* sii = VideoCaptureGpuChannelHost::GetInstance().SharedImageInterface();
  if (!sii) {
    LOG(ERROR) << "Failed to get SharedImageInterface.";
    return false;
  }

  // Setting some default usage in order to get a mappable shared image.
  const auto si_usage =
      gpu::SHARED_IMAGE_USAGE_CPU_WRITE | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  const auto si_format = viz::GetSharedImageFormat(*gfx_format);
  shared_image_ = sii->CreateSharedImage(
      {si_format, dimensions, gfx::ColorSpace(),
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

  std::optional<gfx::BufferFormat> gfx_format = ToBufferFormat(format);
  if (!gfx_format) {
    return false;
  }
  return (viz::GetSharedImageFormat(*gfx_format) == shared_image_->format() &&
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
