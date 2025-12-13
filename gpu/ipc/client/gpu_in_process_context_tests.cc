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
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/351775836): Move ChromeOS to use TestGpuServiceHolder.
#if BUILDFLAG(IS_CHROMEOS)
#include "gpu/ipc/in_process_gpu_thread_holder.h"
#else
#include "components/viz/test/test_gpu_service_holder.h"
#endif

namespace {

class ContextTestBase : public testing::Test {
 public:
  std::unique_ptr<gpu::GLInProcessContext> CreateGLInProcessContext() {
    auto context = std::make_unique<gpu::GLInProcessContext>();
    // TODO(crbug.com/351775836): Move ChromeOS to use TestGpuServiceHolder.
#if BUILDFLAG(IS_CHROMEOS)
    auto result = context->Initialize(gpu_thread_holder_.GetTaskExecutor()
#else
    auto result = context->Initialize(gpu_thread_holder_.task_executor()
#endif
    );
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
  // TODO(crbug.com/351775836): Move ChromeOS to use TestGpuServiceHolder.
#if BUILDFLAG(IS_CHROMEOS)
  gpu::InProcessGpuThreadHolder gpu_thread_holder_;
#else
  viz::TestGpuServiceHolder gpu_thread_holder_;
#endif
  std::unique_ptr<gpu::GLInProcessContext> context_;
};

}  // namespace

// Include the actual tests.
#define CONTEXT_TEST_F TEST_F
#include "gpu/ipc/client/gpu_context_tests.h"
