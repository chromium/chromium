// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/gl_in_process_context.h"

#include <GLES2/gl2.h>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <utility>

#include "base/logging.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/gpu_task_scheduler_helper.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/skia_bindings/gles2_implementation_with_grcontext_support.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/gl/android/surface_texture.h"
#endif

namespace gpu {

GLInProcessContext::GLInProcessContext() = default;

GLInProcessContext::~GLInProcessContext() = default;

const Capabilities& GLInProcessContext::GetCapabilities() const {
  return command_buffer_->GetCapabilities();
}

const GpuFeatureInfo& GLInProcessContext::GetGpuFeatureInfo() const {
  return command_buffer_->GetGpuFeatureInfo();
}

gles2::GLES2Implementation* GLInProcessContext::GetImplementation() {
  return gles2_implementation_.get();
}

SharedImageInterface* GLInProcessContext::GetSharedImageInterface() {
  return command_buffer_->GetSharedImageInterface();
}

ContextResult GLInProcessContext::Initialize(
    CommandBufferTaskExecutor* task_executor,
    const ContextCreationAttribs& attribs,
    const SharedMemoryLimits& mem_limits) {
  DCHECK(base::SingleThreadTaskRunner::GetCurrentDefault());

  command_buffer_ = std::make_unique<InProcessCommandBuffer>(
      task_executor, GURL("chrome://gpu/GLInProcessContext::Initialize"));

  auto result = command_buffer_->Initialize(
      attribs, base::SingleThreadTaskRunner::GetCurrentDefault(),
      /*gr_shader_cache=*/nullptr,
      /*use_shader_cache_shm_count=*/nullptr);
  if (result != ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to initialize InProcessCommmandBuffer";
    return result;
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_ =
      std::make_unique<gles2::GLES2CmdHelper>(command_buffer_.get());
  result = gles2_helper_->Initialize(mem_limits.command_buffer_size);
  if (result != ContextResult::kSuccess) {
    LOG(ERROR) << "Failed to initialize GLES2CmdHelper";
    return result;
  }

  // Create a transfer buffer.
  transfer_buffer_ = std::make_unique<TransferBuffer>(gles2_helper_.get());

  // Check for consistency.
  DCHECK(!attribs.bind_generates_resource);
  const bool bind_generates_resource = false;
  const bool support_client_side_arrays = false;

  // Create the object exposing the OpenGL API.
  gles2_implementation_ =
      std::make_unique<skia_bindings::GLES2ImplementationWithGrContextSupport>(
          gles2_helper_.get(), /*share_group=*/nullptr, transfer_buffer_.get(),
          bind_generates_resource, attribs.lose_context_when_out_of_memory,
          support_client_side_arrays, command_buffer_.get());

  result = gles2_implementation_->Initialize(mem_limits);
  return result;
}

}  // namespace gpu
