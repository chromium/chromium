// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_CPU_READBACK_UPLOAD_COPY_STRATEGY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_CPU_READBACK_UPLOAD_COPY_STRATEGY_H_

#include "gpu/command_buffer/service/shared_image/shared_image_copy_strategy.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

// A CPU-based copy strategy that handles:
// 1. Direct 1-hop copies between a SharedMemoryImageBacking and a non-shared-
//    memory backing using `SharedMemoryImageBacking::pixmaps()` without
//    allocating intermediate staging buffers.
// 2. Universal 2-hop fallback copies between two non-shared-memory backings
//    via CPU readback into temporary SkBitmaps followed by CPU upload.
class GPU_GLES2_EXPORT CPUReadbackUploadCopyStrategy
    : public SharedImageCopyStrategy {
 public:
  CPUReadbackUploadCopyStrategy();
  ~CPUReadbackUploadCopyStrategy() override;

  // SharedImageCopyStrategy implementation:
  bool CanCopy(SharedImageBacking* src_backing,
               SharedImageBacking* dst_backing) override;
  bool Copy(SharedImageBacking* src_backing,
            SharedImageBacking* dst_backing) override;
  void CopyAsync(SharedImageBacking* src_backing,
                 SharedImageBacking* dst_backing,
                 base::OnceCallback<void(bool)> callback) override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_CPU_READBACK_UPLOAD_COPY_STRATEGY_H_
