// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_BUFFER_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_BUFFER_FACTORY_H_

#include <memory>
#include <unordered_map>

#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/pixel_format_utils.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

class CAPTURE_EXPORT CameraBufferFactory {
 public:
  CameraBufferFactory();

  virtual ~CameraBufferFactory();

  virtual std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format);

  virtual ChromiumPixelFormat ResolveStreamBufferFormat(
      cros::mojom::HalPixelFormat hal_format);

  static gfx::BufferUsage GetBufferUsage(gfx::BufferFormat format);

 private:
  std::unordered_map<cros::mojom::HalPixelFormat, ChromiumPixelFormat>
      resolved_hal_formats_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_BUFFER_FACTORY_H_
