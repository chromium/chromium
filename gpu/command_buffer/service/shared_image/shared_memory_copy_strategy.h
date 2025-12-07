// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_MEMORY_COPY_STRATEGY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_MEMORY_COPY_STRATEGY_H_

#include "gpu/command_buffer/service/shared_image/shared_image_copy_strategy.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

// A copy strategy that copies to/from a SharedMemoryImageBacking.
class GPU_GLES2_EXPORT SharedMemoryCopyStrategy
    : public SharedImageCopyStrategy {
 public:
  SharedMemoryCopyStrategy();
  ~SharedMemoryCopyStrategy() override;

  // SharedImageCopyStrategy implementation:
  bool CanCopy(SharedImageBacking* src_backing,
               SharedImageBacking* dst_backing) override;
  bool Copy(SharedImageBacking* src_backing,
            SharedImageBacking* dst_backing) override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_MEMORY_COPY_STRATEGY_H_
