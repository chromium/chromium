// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_shared_image.h"

namespace gpu {

ClientSharedImage::ClientSharedImage(const Mailbox& mailbox)
    : ClientSharedImage(mailbox, nullptr) {}

ClientSharedImage::ClientSharedImage(
    const Mailbox& mailbox,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer)
    : mailbox_(mailbox), gpu_memory_buffer_(std::move(gpu_memory_buffer)) {}

ClientSharedImage::~ClientSharedImage() = default;

std::unique_ptr<SharedImageInterface::ScopedMapping> ClientSharedImage::Map() {
  auto scoped_mapping =
      SharedImageInterface::ScopedMapping::Create(gpu_memory_buffer_.get());
  if (!scoped_mapping) {
    LOG(ERROR) << "Unable to create ScopedMapping";
  }
  return scoped_mapping;
}

}  // namespace gpu
