// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_BUFFER_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_BUFFER_FACTORY_H_

#include <map>
#include <memory>

#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/pixel_format_utils.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
class ClientSharedImage;
}

namespace media {

class CAPTURE_EXPORT CameraBufferFactory {
 public:
  CameraBufferFactory();

  virtual ~CameraBufferFactory();

  virtual std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage);

  virtual scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const gfx::ColorSpace& color_space = gfx::ColorSpace());

  virtual scoped_refptr<gpu::ClientSharedImage> CreateSharedImageFromGmbHandle(
      gfx::GpuMemoryBufferHandle buffer_handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const gfx::ColorSpace& color_space = gfx::ColorSpace());

  virtual ChromiumPixelFormat ResolveStreamBufferFormat(
      cros::mojom::HalPixelFormat hal_format,
      gfx::BufferUsage usage);

 private:
  std::map<std::pair<cros::mojom::HalPixelFormat, gfx::BufferUsage>,
           ChromiumPixelFormat>
      resolved_format_usages_;

  gpu::GpuMemoryBufferSupport gpu_memory_buffer_support_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_BUFFER_FACTORY_H_
