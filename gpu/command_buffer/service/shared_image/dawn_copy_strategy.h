// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_COPY_STRATEGY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_COPY_STRATEGY_H_

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image/shared_image_copy_strategy.h"
#include "gpu/gpu_gles2_export.h"

namespace wgpu {
class Device;
class Texture;
}  // namespace wgpu

namespace gpu {

class SharedImageBacking;
class DawnImageBacking;

// This class implements a copy strategy to copy between GPU backings and
// DawnImageBacking. It uses a WebGPU staging buffer for the copy.
class GPU_GLES2_EXPORT DawnCopyStrategy : public SharedImageCopyStrategy {
 public:
  DawnCopyStrategy();
  ~DawnCopyStrategy() override;

  DawnCopyStrategy(const DawnCopyStrategy&) = delete;
  DawnCopyStrategy& operator=(const DawnCopyStrategy&) = delete;

  // SharedImageCopyStrategy implementation.
  bool CanCopy(SharedImageBacking* src_backing,
               SharedImageBacking* dst_backing) override;
  bool Copy(SharedImageBacking* src_backing,
            SharedImageBacking* dst_backing) override;

  // Helper to copy from a GPU accelerated backing to a Dawn texture.
  static bool CopyFromBackingToTexture(SharedImageBacking* src_backing,
                                       wgpu::Texture dst_texture,
                                       wgpu::Device device);

  // Helper to copy from a Dawn texture to a GPU accelerated backing.
  static bool CopyFromTextureToBacking(wgpu::Texture src_texture,
                                       SharedImageBacking* dst_backing,
                                       wgpu::Device device);

 private:
  bool CopyFromGpuBackingToDawn(SharedImageBacking* src, DawnImageBacking* dst);
  bool CopyFromDawnToGpuBacking(DawnImageBacking* src, SharedImageBacking* dst);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_COPY_STRATEGY_H_
