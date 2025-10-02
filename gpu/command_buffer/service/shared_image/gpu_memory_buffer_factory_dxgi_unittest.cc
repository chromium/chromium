// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gpu_memory_buffer_factory_dxgi.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gpu {

class GpuMemoryBufferFactoryDXGITest : public testing::Test {
 public:
  // Overridden from testing::Test:
  void SetUp() override {
    // This test only works with hardware rendering.
    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseGpuInTests));
    display_ = gl::GLSurfaceTestSupport::InitializeOneOff();
  }
  void TearDown() override { gl::GLSurfaceTestSupport::ShutdownGL(display_); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  GpuMemoryBufferFactoryDXGI factory_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

// Disabled by default as it requires DX11.
TEST_F(GpuMemoryBufferFactoryDXGITest, DISABLED_CreateGpuMemoryBuffer) {
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
              IsNativeGpuMemoryBufferConfigurationSupportedForTesting(format,
                                                                      usage)) {
        continue;
      }

      gfx::GpuMemoryBufferHandle handle = factory_.CreateNativeGmbHandle(
          gfx::Size(2, 2), viz::GetSharedImageFormat(format), usage);
      EXPECT_EQ(handle.type, gfx::DXGI_SHARED_HANDLE);
    }
  }
}

}  // namespace gpu
