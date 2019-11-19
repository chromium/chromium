// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_buffer_factory.h"

#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"

namespace media {

CameraBufferFactory::CameraBufferFactory() = default;

CameraBufferFactory::~CameraBufferFactory() = default;

std::unique_ptr<gfx::GpuMemoryBuffer>
CameraBufferFactory::CreateGpuMemoryBuffer(const gfx::Size& size,
                                           gfx::BufferFormat format) {
  gpu::GpuMemoryBufferManager* buf_manager =
      VideoCaptureDeviceFactoryChromeOS::GetBufferManager();
  if (!buf_manager) {
    LOG(ERROR) << "GpuMemoryBufferManager not set";
    return std::unique_ptr<gfx::GpuMemoryBuffer>();
  }
  return buf_manager->CreateGpuMemoryBuffer(
      size, format, GetBufferUsage(format), gpu::kNullSurfaceHandle);
}

// There's no good way to resolve the HAL pixel format to the platform-specific
// DRM format, other than to actually allocate the buffer and see if the
// allocation succeeds.
ChromiumPixelFormat CameraBufferFactory::ResolveStreamBufferFormat(
    cros::mojom::HalPixelFormat hal_format) {
  if (resolved_hal_formats_.find(hal_format) != resolved_hal_formats_.end()) {
    return resolved_hal_formats_[hal_format];
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
    auto buffer = CreateGpuMemoryBuffer(
        gfx::Size(kDummyBufferWidth, kDummyBufferHeight), f.gfx_format);
    if (buffer) {
      resolved_hal_formats_[hal_format] = f;
      return f;
    }
  }
  return kUnsupportedFormat;
}

// static
gfx::BufferUsage CameraBufferFactory::GetBufferUsage(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      // Usage for JPEG capture buffer backed by R8 pixel buffer.
      return gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE;
    default:
      // Default usage for YUV camera buffer.
      return gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE;
  }
}

}  // namespace media
