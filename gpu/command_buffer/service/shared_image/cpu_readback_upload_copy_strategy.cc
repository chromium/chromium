// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/cpu_readback_upload_copy_strategy.h"

#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace gpu {

CPUReadbackUploadCopyStrategy::CPUReadbackUploadCopyStrategy() = default;
CPUReadbackUploadCopyStrategy::~CPUReadbackUploadCopyStrategy() = default;

bool CPUReadbackUploadCopyStrategy::CanCopy(SharedImageBacking* src_backing,
                                            SharedImageBacking* dst_backing) {
  // TODO(crbug.com/434215964): Add IsSupportedReadbackToMemory() and
  // IsSupportedUploadFromMemory() as virtual method to SharedImageBacking
  // and use it here to determine if this strategy can be supported or not.
  return src_backing->format() == dst_backing->format() &&
         src_backing->size() == dst_backing->size();
}

bool CPUReadbackUploadCopyStrategy::Copy(SharedImageBacking* src_backing,
                                         SharedImageBacking* dst_backing) {
  std::vector<SkBitmap> bitmaps;
  for (int i = 0; i < src_backing->format().NumberOfPlanes(); ++i) {
    SkImageInfo info = src_backing->AsSkImageInfo(i);
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
