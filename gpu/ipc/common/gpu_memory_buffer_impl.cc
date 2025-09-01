// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl.h"

#include "ui/gfx/buffer_format_util.h"

namespace gpu {

GpuMemoryBufferImpl::GpuMemoryBufferImpl() = default;

GpuMemoryBufferImpl::~GpuMemoryBufferImpl() = default;

void GpuMemoryBufferImpl::MapAsync(base::OnceCallback<void(bool)> result_cb) {
  std::move(result_cb).Run(Map());
}

bool GpuMemoryBufferImpl::AsyncMappingIsNonBlocking() const {
  return false;
}

}  // namespace gpu
