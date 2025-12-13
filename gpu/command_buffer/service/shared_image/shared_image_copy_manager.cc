// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_copy_manager.h"

#include <memory>

#include "base/logging.h"
#include "gpu/command_buffer/service/shared_image/shared_image_copy_strategy.h"

namespace gpu {

SharedImageCopyManager::SharedImageCopyManager() = default;
SharedImageCopyManager::~SharedImageCopyManager() = default;

void SharedImageCopyManager::AddStrategy(
    std::unique_ptr<SharedImageCopyStrategy> strategy) {
  strategies_.push_back(std::move(strategy));
}

bool SharedImageCopyManager::CopyImage(SharedImageBacking* src_backing,
                                       SharedImageBacking* dst_backing) {
  for (const auto& strategy : strategies_) {
    if (strategy->CanCopy(src_backing, dst_backing)) {
      return strategy->Copy(src_backing, dst_backing);
    }
  }

  LOG(ERROR) << "No supported copy strategy found for the given backings.";
  return false;
}

}  // namespace gpu
