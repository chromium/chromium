// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "gpu/ipc/in_process_gpu_thread_holder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ContextTestBase : public testing::Test {
 public:
  std::unique_ptr<gpu::GLInProcessContext> CreateGLInProcessContext() {
    gpu::ContextCreationAttribs attributes;
    attributes.bind_generates_resource = false;

    auto context = std::make_unique<gpu::GLInProcessContext>();
    auto result = context->Initialize(gpu_thread_holder_.GetTaskExecutor(),
                                      attributes, gpu::SharedMemoryLimits());
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);
    return context;
  }

  void SetUp() override {
    context_ = CreateGLInProcessContext();
    gl_ = context_->GetImplementation();
    context_support_ = context_->GetImplementation();
  }

  void TearDown() override {
    gl_ = nullptr;
    context_support_ = nullptr;
    context_.reset();
  }

 protected:
  raw_ptr<gpu::gles2::GLES2Interface> gl_;
  raw_ptr<gpu::ContextSupport> context_support_;

 private:
  gpu::InProcessGpuThreadHolder gpu_thread_holder_;
  std::unique_ptr<gpu::GLInProcessContext> context_;
};

}  // namespace

// Include the actual tests.
#define CONTEXT_TEST_F TEST_F
#include "gpu/ipc/client/gpu_context_tests.h"
