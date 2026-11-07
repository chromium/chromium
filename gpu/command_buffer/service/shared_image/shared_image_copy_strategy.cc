// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_copy_strategy.h"

#include <utility>

#include "base/functional/callback.h"

namespace gpu {

SharedImageCopyStrategy::~SharedImageCopyStrategy() = default;

void SharedImageCopyStrategy::CopyAsync(
    SharedImageBacking* src_backing,
    SharedImageBacking* dst_backing,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(Copy(src_backing, dst_backing));
}

}  // namespace gpu
