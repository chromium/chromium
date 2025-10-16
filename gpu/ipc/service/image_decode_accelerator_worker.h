// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_
#define GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/config/gpu_info.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gpu {

// An ImageDecodeAcceleratorWorker handles the actual hardware-accelerated
// decode of an image of a specific type (e.g., JPEG, WebP, etc.).
class ImageDecodeAcceleratorWorker {
 public:
  virtual ~ImageDecodeAcceleratorWorker() {}

  // Encapsulates the result of a decode request.
  struct DecodeResult {
    gfx::GpuMemoryBufferHandle handle;
    gfx::Size visible_size;
    viz::SharedImageFormat si_format;
    size_t buffer_byte_size;
    SkYUVColorSpace yuv_color_space;
  };

  using CompletedDecodeCB =
      base::OnceCallback<void(std::unique_ptr<DecodeResult>)>;

  // Returns the profiles supported by this worker. A worker is allowed to
  // support multiple image types (e.g., JPEG and WebP), but only one
  // ImageDecodeAcceleratorSupportedProfile should be returned per supported
  // image type. If the supported profiles can't be computed, an empty vector is
  // returned.
  virtual std::vector<ImageDecodeAcceleratorSupportedProfile>
  GetSupportedProfiles() = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_
