// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GpuMemoryBufferFactory should
// pass in order to be conformant.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"

#if defined(OS_WIN) || defined(USE_OZONE)
#include "base/command_line.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"
#endif

namespace gpu {

template <typename GpuMemoryBufferFactoryType>
class GpuMemoryBufferFactoryTest : public testing::Test {
 public:
#if defined(OS_WIN) || defined(USE_OZONE)
  // Overridden from testing::Test:
  void SetUp() override {
#if defined(OS_WIN)
    // This test only works with hardware rendering.
    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseGpuInTests));
#endif
    gl::GLSurfaceTestSupport::InitializeOneOff();
  }
  void TearDown() override { gl::init::ShutdownGL(false); }
#endif  // defined(OS_WIN) || defined(USE_OZONE)

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  GpuMemoryBufferFactoryType factory_;
};

TYPED_TEST_SUITE_P(GpuMemoryBufferFactoryTest);

TYPED_TEST_P(GpuMemoryBufferFactoryTest, CreateGpuMemoryBuffer) {
  const gfx::GpuMemoryBufferId kBufferId(1);
  const int kClientId = 1;

  gfx::Size buffer_size(2, 2);
  GpuMemoryBufferSupport support;

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    gfx::BufferUsage usages[] = {
        gfx::BufferUsage::GPU_READ,
        gfx::BufferUsage::SCANOUT,
        gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
        gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_VDA_WRITE,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE,
    };
    for (auto usage : usages) {
      if (!support.IsNativeGpuMemoryBufferConfigurationSupported(format, usage))
        continue;

      gfx::GpuMemoryBufferHandle handle =
          TestFixture::factory_.CreateGpuMemoryBuffer(kBufferId, buffer_size,
                                                      format, usage, kClientId,
                                                      gpu::kNullSurfaceHandle);
      EXPECT_NE(handle.type, gfx::EMPTY_BUFFER);
      TestFixture::factory_.DestroyGpuMemoryBuffer(kBufferId, kClientId);
    }
  }
}

// The GpuMemoryBufferFactoryTest test case verifies behavior that is expected
// from a GpuMemoryBuffer factory in order to be conformant.
REGISTER_TYPED_TEST_SUITE_P(GpuMemoryBufferFactoryTest, CreateGpuMemoryBuffer);

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_
