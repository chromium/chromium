// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl_io_surface.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_test_template.h"

namespace gpu {
namespace {

INSTANTIATE_TYPED_TEST_SUITE_P(GpuMemoryBufferImplIOSurface,
                               GpuMemoryBufferImplTest,
                               GpuMemoryBufferImplIOSurface);

}  // namespace
}  // namespace gpu
