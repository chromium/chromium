// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_
#define GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "gpu/config/gpu_info.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

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
    gfx::BufferFormat buffer_format;
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

  // Enqueue a decode of |encoded_data|. The |decode_cb| is called
  // asynchronously when the decode completes passing as parameter DecodeResult
  // containing a reference to the decoded image (in the form of a
  // gfx::GpuMemoryBufferHandle). The |buffer_byte_size| is the size of the
  // buffer that |handle| refers to. For a successful decode, implementations
  // must guarantee that |visible_size| == |output_size|.
  //
  // If the decode fails, |decode_cb| is called asynchronously with nullptr.
  // Callbacks should be called in the order that this method is called.
  virtual void Decode(std::vector<uint8_t> encoded_data,
                      const gfx::Size& output_size,
                      CompletedDecodeCB decode_cb) = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_
