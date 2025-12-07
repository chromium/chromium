// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_COPY_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_COPY_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class SharedImageCopyStrategy;

// Manages copy strategies and performs copies between shared image backings.
// SharedImageBacking can be created and destroyed in different threads. Since
// CompoundImageBacking has a ref to SharedImageCopyManager, it is
// made RefCountedThreadSafe to allow it to be created and destroyed on
// different threads. The |strategies_| does not need locks because they are
// assumed to be added during initialization and not modified afterward.
class GPU_GLES2_EXPORT SharedImageCopyManager
    : public base::RefCountedThreadSafe<SharedImageCopyManager> {
 public:
  SharedImageCopyManager();

  // Adds a strategy to the list of available copy strategies. The manager
  // takes ownership of the strategy. Strategies should be added in order of
  // preference, from most to least optimal.
  void AddStrategy(std::unique_ptr<SharedImageCopyStrategy> strategy);

  // Performs a copy from the source to the destination backing by iterating
  // through the registered strategies and using the first one that supports the
  // given backings. Returns true on success, false otherwise.
  bool CopyImage(SharedImageBacking* src_backing,
                 SharedImageBacking* dst_backing);

 private:
  friend class base::RefCountedThreadSafe<SharedImageCopyManager>;
  ~SharedImageCopyManager();

  std::vector<std::unique_ptr<SharedImageCopyStrategy>> strategies_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_COPY_MANAGER_H_
