// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory_test_template.h"

namespace gpu {
namespace {

INSTANTIATE_TYPED_TEST_SUITE_P(GpuMemoryBufferFactoryIOSurface,
                               GpuMemoryBufferFactoryTest,
                               GpuMemoryBufferFactoryIOSurface);

}  // namespace
}  // namespace gpu
