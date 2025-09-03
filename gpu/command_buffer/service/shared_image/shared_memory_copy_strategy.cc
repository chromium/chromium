// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_memory_copy_strategy.h"

#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing.h"

namespace gpu {

SharedMemoryCopyStrategy::SharedMemoryCopyStrategy() = default;
SharedMemoryCopyStrategy::~SharedMemoryCopyStrategy() = default;

bool SharedMemoryCopyStrategy::CanCopy(SharedImageBacking* src_backing,
                                       SharedImageBacking* dst_backing) {
  bool src_is_shm =
      src_backing->GetType() == SharedImageBackingType::kSharedMemory;
  bool dst_is_shm =
      dst_backing->GetType() == SharedImageBackingType::kSharedMemory;

  // This strategy applies if one of the backings is shared memory, but not
  // both.
  if (src_is_shm == dst_is_shm) {
    return false;
  }

  // TODO(crbug.com/434215964): We should also check if the non-shared-memory
  // backing supports the required UploadFromMemory or ReadbackToMemory
  // operation.

  return src_backing->format() == dst_backing->format() &&
         src_backing->size() == dst_backing->size();
}

bool SharedMemoryCopyStrategy::Copy(SharedImageBacking* src_backing,
                                    SharedImageBacking* dst_backing) {
  bool src_is_shm =
      src_backing->GetType() == SharedImageBackingType::kSharedMemory;

  if (src_is_shm) {
    // Copy from Shared Memory to other backing.
    auto* src_shm_backing = static_cast<SharedMemoryImageBacking*>(src_backing);
    return dst_backing->UploadFromMemory(src_shm_backing->pixmaps());
  } else {
    // Copy from other backing to Shared Memory.
    auto* dst_shm_backing = static_cast<SharedMemoryImageBacking*>(dst_backing);
    return src_backing->ReadbackToMemory(dst_shm_backing->pixmaps());
  }
}

}  // namespace gpu
