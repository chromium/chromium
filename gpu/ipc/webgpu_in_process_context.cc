// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/webgpu_in_process_context.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/test/test_simple_task_runner.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/surface_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

WebGPUInProcessContext::WebGPUInProcessContext() = default;

WebGPUInProcessContext::~WebGPUInProcessContext() {
  // Trigger any pending lost contexts. First do a full sync between client
  // and service threads. Then execute any pending tasks.
  if (webgpu_implementation_) {
    // TODO(crbug.com/40586882): do the equivalent of a glFinish here?
    client_task_runner_->RunUntilIdle();
    webgpu_implementation_.reset();
  }
  transfer_buffer_.reset();
  helper_.reset();
  command_buffer_.reset();
}

ContextResult WebGPUInProcessContext::Initialize(
    CommandBufferTaskExecutor* task_executor,
    const ContextCreationAttribs& attribs,
    const SharedMemoryLimits& memory_limits) {
  DCHECK(attribs.context_type == CONTEXT_TYPE_WEBGPU);

  if (attribs.context_type != CONTEXT_TYPE_WEBGPU ||
      attribs.enable_raster_interface || attribs.enable_gles2_interface) {
    return ContextResult::kFatalFailure;
  }

  client_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  command_buffer_ =
      std::make_unique<InProcessCommandBuffer>(task_executor, GURL());

  auto result =
      command_buffer_->Initialize(attribs, client_task_runner_,
                                  /*gr_shader_cache=*/nullptr,
                                  /*use_shader_cache_shm_count=*/nullptr);
  if (result != ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to initialize InProcessCommmandBuffer";
    return result;
  }

  // Check for consistency.
  DCHECK(!attribs.bind_generates_resource);

  // Create the WebGPUCmdHelper, which writes the command buffer protocol.
  auto webgpu_helper =
      std::make_unique<webgpu::WebGPUCmdHelper>(command_buffer_.get());
  result = webgpu_helper->Initialize(memory_limits.command_buffer_size);
  if (result != ContextResult::kSuccess) {
    LOG(ERROR) << "Failed to initialize WebGPUCmdHelper";
    return result;
  }
  transfer_buffer_ = std::make_unique<TransferBuffer>(webgpu_helper.get());

  webgpu_implementation_ = std::make_unique<webgpu::WebGPUImplementation>(
      webgpu_helper.get(), transfer_buffer_.get(), command_buffer_.get());
  helper_ = std::move(webgpu_helper);
  webgpu_implementation_->Initialize(memory_limits);
  return result;
}

const Capabilities& WebGPUInProcessContext::GetCapabilities() const {
  return command_buffer_->GetCapabilities();
}

const GpuFeatureInfo& WebGPUInProcessContext::GetGpuFeatureInfo() const {
  return command_buffer_->GetGpuFeatureInfo();
}

webgpu::WebGPUImplementation* WebGPUInProcessContext::GetImplementation() {
  return webgpu_implementation_.get();
}

base::TestSimpleTaskRunner* WebGPUInProcessContext::GetTaskRunner() {
  return client_task_runner_.get();
}

ServiceTransferCache* WebGPUInProcessContext::GetTransferCacheForTest() const {
  return command_buffer_->GetTransferCacheForTest();
}

InProcessCommandBuffer* WebGPUInProcessContext::GetCommandBufferForTest()
    const {
  return command_buffer_.get();
}

CommandBufferHelper* WebGPUInProcessContext::GetCommandBufferHelperForTest()
    const {
  return helper_.get();
}

}  // namespace gpu
