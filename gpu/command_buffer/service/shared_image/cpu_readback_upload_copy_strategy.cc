// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/cpu_readback_upload_copy_strategy.h"

#include <vector>

#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace gpu {

CPUReadbackUploadCopyStrategy::CPUReadbackUploadCopyStrategy() = default;
CPUReadbackUploadCopyStrategy::~CPUReadbackUploadCopyStrategy() = default;

bool CPUReadbackUploadCopyStrategy::CanCopy(SharedImageBacking* src_backing,
                                            SharedImageBacking* dst_backing) {
  if (src_backing->GetType() == SharedImageBackingType::kSharedMemory &&
      dst_backing->GetType() == SharedImageBackingType::kSharedMemory) {
    return false;
  }

  // Cannot CPU-upload into an external-sampler multiplanar backing.
  if (dst_backing->format().PrefersExternalSampler()) {
    return false;
  }

  // TODO(crbug.com/434215964): Add IsSupportedReadbackToMemory() and
  // IsSupportedUploadFromMemory() as virtual method to SharedImageBacking
  // and use it here to determine if this strategy can be supported or not.
  return GetEffectiveCopyFormat(src_backing->format()) ==
             dst_backing->format() &&
         src_backing->size() == dst_backing->size();
}

bool CPUReadbackUploadCopyStrategy::Copy(SharedImageBacking* src_backing,
                                         SharedImageBacking* dst_backing) {
  const bool src_is_shm =
      src_backing->GetType() == SharedImageBackingType::kSharedMemory;
  const bool dst_is_shm =
      dst_backing->GetType() == SharedImageBackingType::kSharedMemory;
  if (src_is_shm && dst_is_shm) {
    return false;
  }
  if (src_is_shm) {
    auto* src_shm_backing = static_cast<SharedMemoryImageBacking*>(src_backing);
    return dst_backing->UploadFromMemory(src_shm_backing->pixmaps());
  }
  if (dst_is_shm) {
    auto* dst_shm_backing = static_cast<SharedMemoryImageBacking*>(dst_backing);
    return src_backing->ReadbackToMemory(dst_shm_backing->pixmaps());
  }

  // When `src_backing` uses external sampling, GPU readback samples YUV to a
  // single RGBA plane matching `dst_backing->format()`.
  const SharedImageBacking* layout_backing =
      src_backing->format().PrefersExternalSampler() ? dst_backing
                                                     : src_backing;
  std::vector<SkBitmap> bitmaps;
  for (int i = 0; i < layout_backing->format().NumberOfPlanes(); ++i) {
    SkImageInfo info = layout_backing->AsSkImageInfo(i);
    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(info)) {
      return false;
    }
    bitmaps.push_back(bitmap);
  }

  std::vector<SkPixmap> pixmaps;
  for (const auto& bitmap : bitmaps) {
    pixmaps.push_back(bitmap.pixmap());
  }

  if (!src_backing->ReadbackToMemory(pixmaps)) {
    return false;
  }

  if (!dst_backing->UploadFromMemory(pixmaps)) {
    return false;
  }

  return true;
}

}  // namespace gpu
