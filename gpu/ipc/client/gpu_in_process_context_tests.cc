// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "gpu/ipc/in_process_gpu_thread_holder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ContextTestBase : public testing::Test {
 public:
  std::unique_ptr<gpu::GLInProcessContext> CreateGLInProcessContext() {
    gpu::ContextCreationAttribs attributes;
    attributes.alpha_size = 8;
    attributes.depth_size = 24;
    attributes.red_size = 8;
    attributes.green_size = 8;
    attributes.blue_size = 8;
    attributes.stencil_size = 8;
    attributes.samples = 4;
    attributes.sample_buffers = 1;
    attributes.bind_generates_resource = false;

    auto context = std::make_unique<gpu::GLInProcessContext>();
    auto result = context->Initialize(
        gpu_thread_holder_.GetTaskExecutor(),
        /*surface=*/nullptr, /*offscreen=*/true,
        /*window=*/gpu::kNullSurfaceHandle, attributes,
        gpu::SharedMemoryLimits(), gpu_memory_buffer_manager_.get(),
        /*image_factory=*/nullptr, base::ThreadTaskRunnerHandle::Get());
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);
    return context;
  }

  void SetUp() override {
    gpu_memory_buffer_manager_ =
        std::make_unique<viz::TestGpuMemoryBufferManager>();
    context_ = CreateGLInProcessContext();
    gl_ = context_->GetImplementation();
    context_support_ = context_->GetImplementation();
  }

  void TearDown() override {
    context_.reset();
    gpu_memory_buffer_manager_.reset();
  }

 protected:
  gpu::gles2::GLES2Interface* gl_;
  gpu::ContextSupport* context_support_;
  std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;

 private:
  gpu::InProcessGpuThreadHolder gpu_thread_holder_;
  std::unique_ptr<gpu::GLInProcessContext> context_;
};

}  // namespace

// Include the actual tests.
#define CONTEXT_TEST_F TEST_F
#include "gpu/ipc/client/gpu_context_tests.h"

using GLInProcessCommandBufferTest = ContextTestBase;

TEST_F(GLInProcessCommandBufferTest, CreateImage) {
  constexpr gfx::BufferFormat kBufferFormat = gfx::BufferFormat::RGBA_8888;
  constexpr gfx::BufferUsage kBufferUsage = gfx::BufferUsage::SCANOUT;
  constexpr gfx::Size kBufferSize(100, 100);

  // Calling CreateImageCHROMIUM() should allocate an image id starting at 1.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer1 =
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          kBufferSize, kBufferFormat, kBufferUsage, gpu::kNullSurfaceHandle);
  GLuint image_id1 = gl_->CreateImageCHROMIUM(
      gpu_memory_buffer1->AsClientBuffer(), kBufferSize.width(),
      kBufferSize.height(), GL_RGBA);

  EXPECT_GT(image_id1, 0u);

  // Create a second GLInProcessContext that is backed by a different
  // InProcessCommandBuffer. Calling CreateImageCHROMIUM() should return a
  // different id than the first call.
  std::unique_ptr<gpu::GLInProcessContext> context2 =
      CreateGLInProcessContext();
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer2 =
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          kBufferSize, kBufferFormat, kBufferUsage, gpu::kNullSurfaceHandle);
  GLuint image_id2 = context2->GetImplementation()->CreateImageCHROMIUM(
      buffer2->AsClientBuffer(), kBufferSize.width(), kBufferSize.height(),
      GL_RGBA);

  EXPECT_GT(image_id2, 0u);
  EXPECT_NE(image_id1, image_id2);
}
