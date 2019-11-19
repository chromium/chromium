// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/gl_in_process_context.h"

#include <GLES2/gl2.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/skia_bindings/gles2_implementation_with_grcontext_support.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_image.h"

#if defined(OS_ANDROID)
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
    scoped_refptr<gl::GLSurface> surface,
    bool is_offscreen,
    SurfaceHandle window,
    const ContextCreationAttribs& attribs,
    const SharedMemoryLimits& mem_limits,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    ImageFactory* image_factory,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // If a surface is provided, we are running in a webview and should not have
  // a task runner. We must have a task runner in all other cases.
  DCHECK_EQ(!!surface, !task_runner);
  if (surface) {
    DCHECK_EQ(surface->IsOffscreen(), is_offscreen);
    DCHECK_EQ(kNullSurfaceHandle, window);
  }
  DCHECK_GE(attribs.offscreen_framebuffer_size.width(), 0);
  DCHECK_GE(attribs.offscreen_framebuffer_size.height(), 0);

  command_buffer_ = std::make_unique<InProcessCommandBuffer>(
      task_executor, GURL("chrome://gpu/GLInProcessContext::Initialize"));

  auto result =
      command_buffer_->Initialize(surface, is_offscreen, window, attribs,
                                  gpu_memory_buffer_manager, image_factory,
                                  /*gpu_channel_manager_delegate=*/nullptr,
                                  std::move(task_runner), nullptr, nullptr);
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
