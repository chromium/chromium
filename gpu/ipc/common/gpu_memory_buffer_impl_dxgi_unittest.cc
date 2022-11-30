// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_test_template.h"

namespace gpu {
namespace {

// Disabled by default as it requires DX11.
INSTANTIATE_TYPED_TEST_SUITE_P(DISABLED_GpuMemoryBufferImplDXGI,
                               GpuMemoryBufferImplTest,
                               GpuMemoryBufferImplDXGI);
}  // namespace
}  // namespace gpu
