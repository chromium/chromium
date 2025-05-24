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

scoped_refptr<gpu::ClientSharedImage> CameraBufferFactory::CreateSharedImage(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const gfx::ColorSpace& color_space) {
  auto sii = VideoCaptureDeviceFactoryChromeOS::GetSharedImageInterface();
  if (!sii) {
    LOG(ERROR) << "SharedImageInterface not set.";
    return nullptr;
  }

  // In the media capture process, the underlying GMB handle created via the
  // below shared image is only used for CPU read/write. It is then later sent
  // to the renderer which uses the handle to create a new shared image for
  // drawing. Hence there is no need to create and hold a service side GMB
  // handle/NativePixmap as a part of OzoneImageBacking created via below
  // CreateSharedImage call. Creating and holding a NativePixmap via below
  // CreateSharedImage call also fails for R8 format since it's not a
  // texturable format for some devices.
  // Hence we use the special usage flag SHARED_IMAGE_USAGE_CPU_ONLY_READ_WRITE
  // which instructs the service side code that a NativePixmap inside the
  // SharedImage is not necessary for this use case.
  // Note that we'll need to refine this if/when we want to send these
  // SharedImages over to the renderer process when feasible (i.e., for non-R8
  // and/or for R8 on devices where it's texturable).
  auto shared_image = sii->CreateSharedImage(
      {viz::GetSharedImageFormat(format), size, color_space,
       gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_CPU_ONLY_READ_WRITE),
       "CameraBufferFactory"},
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
  auto sii = VideoCaptureDeviceFactoryChromeOS::GetSharedImageInterface();
  if (!sii) {
    LOG(ERROR) << "SharedImageInterface not set.";
    return nullptr;
  }

  // In the media capture process, the underlying GMB handle created via the
  // below shared image is only used for CPU read/write. It is then later sent
  // to the renderer which uses the handle to create a new shared image for
  // drawing. Hence there is no need to create and hold a service side GMB
  // handle/NativePixmap as a part of OzoneImageBacking created via below
  // CreateSharedImage call. Creating and holding a NativePixmap via below
  // CreateSharedImage call also fails for R8 format since it's not a
  // texturable format for some devices.
  // Hence we use the special usage flag SHARED_IMAGE_USAGE_CPU_ONLY_READ_WRITE
  // which instructs the service side code that a NativePixmap inside the
  // SharedImage is not necessary for this use case.
  // Note that we'll need to refine this if/when we want to send these
  // SharedImages over to the renderer process when feasible (i.e., for non-R8
  // and/or for R8 on devices where it's texturable).
  auto shared_image = sii->CreateSharedImage(
      {viz::GetSharedImageFormat(format), size, color_space,
       gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_CPU_ONLY_READ_WRITE),
       "CameraBufferFactory"},
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
