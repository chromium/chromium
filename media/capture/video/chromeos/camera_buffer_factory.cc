// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_buffer_factory.h"

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"

namespace media {

CameraBufferFactory::CameraBufferFactory() = default;

CameraBufferFactory::~CameraBufferFactory() = default;

std::unique_ptr<gfx::GpuMemoryBuffer>
CameraBufferFactory::CreateGpuMemoryBuffer(const gfx::Size& size,
                                           gfx::BufferFormat format,
                                           gfx::BufferUsage usage) {
  // TODO(b/363936240): Check if we can set and use GpuChannelHost in the
  // browser process to create buffers.
  // GpuChannelHost is able to be reset with new channel host to establish the
  // new connection to GPU process via viz::Gpu APIs when GPU process crashes.
  // Therefore, we use GpuChannelHost to create buffers in the utility process.
  // |gpu_channel_host| is a nullptr in the browser process.
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      VideoCaptureDeviceFactoryChromeOS::GetGpuChannelHost();
  if (gpu_channel_host) {
    gfx::GpuMemoryBufferHandle gmb_handle;
    gpu_channel_host->CreateGpuMemoryBuffer(
        size, viz::GetSharedImageFormat(format), usage, &gmb_handle);
    return gpu_memory_buffer_support_.CreateGpuMemoryBufferImplFromHandle(
        std::move(gmb_handle), size, format, usage, base::NullCallback());
  }
  // GpuMemoryBufferManagerSingleton is only available in the browser process.
  // |buf_manager| is a nullptr in the utility process.
  gpu::GpuMemoryBufferManager* buf_manager =
      VideoCaptureDeviceFactoryChromeOS::GetBufferManager();
  if (buf_manager) {
    return buf_manager->CreateGpuMemoryBuffer(size, format, usage,
                                              gpu::kNullSurfaceHandle, nullptr);
  }
  LOG(ERROR)
      << "Both of GpuChannelHost and GpuMemoryBufferManager are not set.";
  return nullptr;
}

// There's no good way to resolve the HAL pixel format to the platform-specific
// DRM format, other than to actually allocate the buffer and see if the
// allocation succeeds.
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
    auto buffer = CreateGpuMemoryBuffer(
        gfx::Size(kDummyBufferWidth, kDummyBufferHeight), f.gfx_format, usage);
    if (buffer) {
      resolved_format_usages_[key] = f;
      return f;
    }
  }
  return kUnsupportedFormat;
}

}  // namespace media
