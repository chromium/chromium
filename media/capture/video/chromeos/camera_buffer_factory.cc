// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_buffer_factory.h"

#include "base/containers/contains.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"

namespace media {

// Setting some default usage in order to get a mappable shared image.
constexpr auto si_usage =
    gpu::SHARED_IMAGE_USAGE_CPU_WRITE | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

CameraBufferFactory::CameraBufferFactory() = default;

CameraBufferFactory::~CameraBufferFactory() = default;

std::unique_ptr<gfx::GpuMemoryBuffer>
CameraBufferFactory::CreateGpuMemoryBuffer(const gfx::Size& size,
                                           gfx::BufferFormat format,
                                           gfx::BufferUsage usage) {
  gpu::GpuMemoryBufferManager* buf_manager =
      VideoCaptureDeviceFactoryChromeOS::GetBufferManager();
  if (!buf_manager) {
    LOG(ERROR) << "GpuMemoryBufferManager not set";
    return nullptr;
  }
  return buf_manager->CreateGpuMemoryBuffer(size, format, usage,
                                            gpu::kNullSurfaceHandle, nullptr);
}

scoped_refptr<gpu::ClientSharedImage> CameraBufferFactory::CreateSharedImage(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const gfx::ColorSpace& color_space) {
  auto* sii = VideoCaptureDeviceFactoryChromeOS::GetSharedImageInterface();
  if (!sii) {
    LOG(ERROR) << "SharedImageInterface not set.";
    return nullptr;
  }

  auto shared_image = sii->CreateSharedImage(
      {viz::GetSharedImageFormat(format), size, color_space,
       gpu::SharedImageUsageSet(si_usage), "CameraBufferFactory"},
      gpu::kNullSurfaceHandle, usage);
  if (!shared_image) {
    LOG(ERROR) << "Failed to create a shared image.";
  }
  return shared_image;
}

scoped_refptr<gpu::ClientSharedImage>
CameraBufferFactory::CreateSharedImageFromGmbHandle(
    gfx::GpuMemoryBufferHandle buffer_handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const gfx::ColorSpace& color_space) {
  auto* sii = VideoCaptureDeviceFactoryChromeOS::GetSharedImageInterface();
  if (!sii) {
    LOG(ERROR) << "SharedImageInterface not set.";
    return nullptr;
  }

  auto shared_image = sii->CreateSharedImage(
      {viz::GetSharedImageFormat(format), size, color_space,
       gpu::SharedImageUsageSet(si_usage), "CameraBufferFactory"},
      gpu::kNullSurfaceHandle, usage, std::move(buffer_handle));
  if (!shared_image) {
    LOG(ERROR) << "Failed to create a shared image.";
  }
  return shared_image;
}

// There's no good way to resolve the HAL pixel format to the
// platform-specific DRM format, other than to actually allocate the buffer
// and see if the allocation succeeds.
ChromiumPixelFormat CameraBufferFactory::ResolveStreamBufferFormat(
    cros::mojom::HalPixelFormat hal_format,
    gfx::BufferUsage usage) {
  const auto key = std::make_pair(hal_format, usage);
  if (base::Contains(resolved_format_usages_, key)) {
    return resolved_format_usages_[key];
  }

  ChromiumPixelFormat kUnsupportedFormat{PIXEL_FORMAT_UNKNOWN,
                                         gfx::BufferFormat::RGBX_8888};
  size_t kDummyBufferWidth = 128, kDummyBufferHeight = 128;
  std::vector<ChromiumPixelFormat> cr_formats =
      PixFormatHalToChromium(hal_format);
  if (cr_formats.empty()) {
    return kUnsupportedFormat;
  }
  for (const auto& f : cr_formats) {
    auto shared_image = CreateSharedImage(
        gfx::Size(kDummyBufferWidth, kDummyBufferHeight), f.gfx_format, usage);
    if (shared_image) {
      resolved_format_usages_[key] = f;
      return f;
    }
  }
  return kUnsupportedFormat;
}

}  // namespace media
