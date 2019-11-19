// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/shared_memory.h"

#include <utility>

#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory.h"

namespace tracing {

std::unique_ptr<perfetto::SharedMemory>
MojoSharedMemory::Factory::CreateSharedMemory(size_t size) {
  return std::make_unique<MojoSharedMemory>(size);
}

MojoSharedMemory::MojoSharedMemory(size_t size) {
  shared_buffer_ = mojo::SharedBufferHandle::Create(size);
  mapping_ = shared_buffer_->Map(size);
  DCHECK(mapping_);
}

MojoSharedMemory::MojoSharedMemory(mojo::ScopedSharedBufferHandle shared_memory)
    : shared_buffer_(std::move(shared_memory)) {
  mapping_ = shared_buffer_->Map(shared_buffer_->GetSize());
  DCHECK(mapping_);
}

MojoSharedMemory::~MojoSharedMemory() = default;

mojo::ScopedSharedBufferHandle MojoSharedMemory::Clone() {
  return shared_buffer_->Clone(
      mojo::SharedBufferHandle::AccessMode::READ_WRITE);
}

void* MojoSharedMemory::start() const {
  return mapping_.get();
}

size_t MojoSharedMemory::size() const {
  return shared_buffer_->GetSize();
}

}  // namespace tracing
