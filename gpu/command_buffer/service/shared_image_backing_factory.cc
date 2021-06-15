// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image_backing.h"

namespace gpu {

std::vector<std::unique_ptr<SharedImageBacking>>
SharedImageBackingFactory::CreateSharedImageVideoPlanes(
    base::span<const Mailbox> mailboxes,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    const gfx::Size& size,
    uint32_t usage) {
  NOTREACHED();
  return {};
}

}  // namespace gpu
