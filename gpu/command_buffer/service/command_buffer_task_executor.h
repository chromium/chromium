// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_TASK_EXECUTOR_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_TASK_EXECUTOR_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/shm_count.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/framebuffer_completeness_cache.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_surface_format.h"

namespace gl {
class GLShareGroup;
}

namespace gpu {
class SyncPointManager;
class SingleTaskSequence;

namespace gles2 {
class Outputter;
class ProgramCache;
}  // namespace gles2

// Provides accessors for GPU service objects and the serializer interface to
// the GPU thread used by InProcessCommandBuffer.
// TODO(crbug.com/40196979): This class should be revisited as lots of
// functionality isn't needed anymore with GLRenderer deleted.
class GPU_GLES2_EXPORT CommandBufferTaskExecutor {
 public:
  CommandBufferTaskExecutor(const GpuPreferences& gpu_preferences,
                            const GpuFeatureInfo& gpu_feature_info,
                            SyncPointManager* sync_point_manager,
                            gl::GLSurfaceFormat share_group_surface_format,
                            SharedImageManager* shared_image_manager,
                            gles2::ProgramCache* program_cache);

  CommandBufferTaskExecutor(const CommandBufferTaskExecutor&) = delete;
  CommandBufferTaskExecutor& operator=(const CommandBufferTaskExecutor&) =
      delete;

  virtual ~CommandBufferTaskExecutor();

  // Always use virtualized GL contexts if this returns true.
  virtual bool ForceVirtualizedGLContexts() const = 0;

  // Creates a memory tracker for the context group if this returns true.
  virtual bool ShouldCreateMemoryTracker() const = 0;

  // Schedules |task| to run out of order with respect to other sequenced tasks.
  virtual void ScheduleOutOfOrderTask(base::OnceClosure task) = 0;

  // Schedules |task| to run at an appropriate time for performing delayed work.
  virtual void ScheduleDelayedWork(base::OnceClosure task) = 0;

  // Returns a new task execution sequence. Sequences should not outlive the
  // task executor.
  virtual std::unique_ptr<SingleTaskSequence> CreateSequence() = 0;

  // Called if InProcessCommandBuffer is not passed a client TaskRunner.
  virtual void PostNonNestableToClient(base::OnceClosure callback) = 0;

  // Returns the shared offscreen context state.
  virtual scoped_refptr<SharedContextState> GetSharedContextState() = 0;

  virtual scoped_refptr<gl::GLShareGroup> GetShareGroup() = 0;

  const GpuPreferences& gpu_preferences() const { return gpu_preferences_; }
  const GpuFeatureInfo& gpu_feature_info() const { return gpu_feature_info_; }
  gl::GLSurfaceFormat share_group_surface_format() const {
    return share_group_surface_format_;
  }
  SyncPointManager* sync_point_manager() const { return sync_point_manager_; }

  // Not const because these return inner pointers.
  ServiceDiscardableManager* discardable_manager() {
    return &discardable_manager_;
  }
  PassthroughDiscardableManager* passthrough_discardable_manager() {
    return &passthrough_discardable_manager_;
  }
  gles2::ShaderTranslatorCache* shader_translator_cache() {
    return &shader_translator_cache_;
  }
  gles2::FramebufferCompletenessCache* framebuffer_completeness_cache() {
    return &framebuffer_completeness_cache_;
  }
  SharedImageManager* shared_image_manager() { return shared_image_manager_; }

  // These methods construct accessed fields if not already initialized.
  gles2::Outputter* outputter();
  gles2::ProgramCache* program_cache();

 private:
  const GpuPreferences gpu_preferences_;
  const GpuFeatureInfo gpu_feature_info_;
  raw_ptr<SyncPointManager> sync_point_manager_;
  std::unique_ptr<gles2::Outputter> outputter_;
  gl::GLSurfaceFormat share_group_surface_format_;
  std::unique_ptr<gles2::ProgramCache> owned_program_cache_;
  raw_ptr<gles2::ProgramCache> program_cache_;
  ServiceDiscardableManager discardable_manager_;
  PassthroughDiscardableManager passthrough_discardable_manager_;
  gles2::ShaderTranslatorCache shader_translator_cache_;
  gles2::FramebufferCompletenessCache framebuffer_completeness_cache_;
  raw_ptr<SharedImageManager> shared_image_manager_;

  // No-op default initialization is used in in-process mode.
  GpuProcessShmCount use_shader_cache_shm_count_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_TASK_EXECUTOR_H_
