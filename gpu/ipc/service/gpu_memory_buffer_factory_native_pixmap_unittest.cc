// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_native_pixmap.h"

#include "build/build_config.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory_test_template.h"

namespace gpu {
namespace {

// On Fuchsia NativePixmap depends on Vulkan, which is not initialized in tests.
// See crbug.com/957700
#if BUILDFLAG(IS_FUCHSIA)
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuMemoryBufferFactoryTest);
#else
INSTANTIATE_TYPED_TEST_SUITE_P(GpuMemoryBufferFactoryNativePixmap,
                               GpuMemoryBufferFactoryTest,
                               GpuMemoryBufferFactoryNativePixmap);
#endif

}  // namespace
}  // namespace gpu
