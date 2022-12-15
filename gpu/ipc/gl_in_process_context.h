// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_GL_IN_PROCESS_CONTEXT_H_
#define GPU_IPC_GL_IN_PROCESS_CONTEXT_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"
#include "gpu/ipc/gl_in_process_context_export.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class SharedImageInterface;
class TransferBuffer;
struct GpuFeatureInfo;
struct SharedMemoryLimits;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}

// Wraps everything needed to use an in process GL context.
class GL_IN_PROCESS_CONTEXT_EXPORT GLInProcessContext {
 public:
  // You must call Initialize() before using the context.
  GLInProcessContext();

  GLInProcessContext(const GLInProcessContext&) = delete;
  GLInProcessContext& operator=(const GLInProcessContext&) = delete;

  ~GLInProcessContext();

  // Initialize the GLInProcessContext, if |is_offscreen| is true, renders to an
  // offscreen context. |attrib_list| must be null or a NONE-terminated list
  // of attribute/value pairs.
  // If |surface| is not null, then it must match |is_offscreen|,
  // |window| must be gfx::kNullAcceleratedWidget, and the command buffer
  // service must run on the same thread as this client because GLSurface is
  // not thread safe. If |surface| is null, then the other parameters are used
  // to correctly create a surface.
  ContextResult Initialize(CommandBufferTaskExecutor* task_executor,
                           const ContextCreationAttribs& attribs,
                           const SharedMemoryLimits& memory_limits);

  const Capabilities& GetCapabilities() const;
  const GpuFeatureInfo& GetGpuFeatureInfo() const;

  // Allows direct access to the GLES2 implementation so a GLInProcessContext
  // can be used without making it current.
  gles2::GLES2Implementation* GetImplementation();

  SharedImageInterface* GetSharedImageInterface();

 private:
  // The destruction order is important, don't reorder these member variables.
  std::unique_ptr<InProcessCommandBuffer> command_buffer_;
  std::unique_ptr<gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  std::unique_ptr<gles2::GLES2Implementation> gles2_implementation_;
};

}  // namespace gpu

#endif  // GPU_IPC_GL_IN_PROCESS_CONTEXT_H_
