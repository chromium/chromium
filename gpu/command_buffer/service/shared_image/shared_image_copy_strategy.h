// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_COPY_STRATEGY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_COPY_STRATEGY_H_

#include "gpu/gpu_gles2_export.h"

namespace gpu {

class SharedImageBacking;

// Interface for copy strategies between different shared image backing types.
class GPU_GLES2_EXPORT SharedImageCopyStrategy {
 public:
  virtual ~SharedImageCopyStrategy() = default;

  // Returns true if this strategy can be used to copy between the given
  // source and destination backings.
  virtual bool CanCopy(SharedImageBacking* src_backing,
                       SharedImageBacking* dst_backing) = 0;

  // Performs a copy from the source to the destination backing.
  // Returns true on success, false otherwise.
  virtual bool Copy(SharedImageBacking* src_backing,
                    SharedImageBacking* dst_backing) = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_COPY_STRATEGY_H_
