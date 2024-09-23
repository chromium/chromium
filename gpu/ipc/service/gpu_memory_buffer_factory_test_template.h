// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GpuMemoryBufferFactory should
// pass in order to be conformant.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_display.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE)
#include "base/command_line.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"
#endif

namespace gpu {

template <typename GpuMemoryBufferFactoryType>
class GpuMemoryBufferFactoryTest : public testing::Test {
 public:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE)
  // Overridden from testing::Test:
  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    // This test only works with hardware rendering.
    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseGpuInTests));
#endif
    display_ = gl::GLSurfaceTestSupport::InitializeOneOff();
  }
  void TearDown() override { gl::GLSurfaceTestSupport::ShutdownGL(display_); }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE)

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  GpuMemoryBufferFactoryType factory_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

TYPED_TEST_SUITE_P(GpuMemoryBufferFactoryTest);

TYPED_TEST_P(GpuMemoryBufferFactoryTest, CreateGpuMemoryBuffer) {
  const gfx::GpuMemoryBufferId kBufferId(1);
  const int kClientId = 1;

  gfx::Size buffer_size(2, 2);

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    gfx::BufferUsage usages[] = {
        gfx::BufferUsage::GPU_READ,
        gfx::BufferUsage::SCANOUT,
        gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
        gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_VDA_WRITE,
        gfx::BufferUsage::PROTECTED_SCANOUT,
        gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_FRONT_RENDERING,
    };
    for (auto usage : usages) {
      if (!gpu::GpuMemoryBufferSupport::
              IsNativeGpuMemoryBufferConfigurationSupported(format, usage)) {
        continue;
      }

      gfx::GpuMemoryBufferHandle handle =
          TestFixture::factory_.CreateGpuMemoryBuffer(
              kBufferId, buffer_size, /*framebuffer_size=*/buffer_size, format,
              usage, kClientId, gpu::kNullSurfaceHandle);
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
