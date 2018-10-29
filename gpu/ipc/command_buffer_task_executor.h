// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMAND_BUFFER_TASK_EXECUTOR_H_
#define GPU_IPC_COMMAND_BUFFER_TASK_EXECUTOR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/framebuffer_completeness_cache.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/gl_in_process_context_export.h"
#include "ui/gl/gl_surface_format.h"

namespace gl {
class GLShareGroup;
}

namespace gpu {
class MailboxManager;
class SyncPointManager;

namespace gles2 {
class Outputter;
class ProgramCache;
}  // namespace gles2

// Provides accessors for GPU service objects and the serializer interface to
// the GPU thread used by InProcessCommandBuffer.
class GL_IN_PROCESS_CONTEXT_EXPORT CommandBufferTaskExecutor
    : public base::RefCountedThreadSafe<CommandBufferTaskExecutor> {
 public:
  // Represents a single task execution sequence. Tasks posted to a sequence are
  // run in order. Tasks across sequences should be synchronized using sync
  // tokens. Destroying the sequence will drop tasks which haven't been executed
  // yet.
  class GL_IN_PROCESS_CONTEXT_EXPORT Sequence {
   public:
    virtual ~Sequence() {}

    // Returns identifier used for identifying sync tokens with this sequence,
    // and for scheduling.
    virtual SequenceId GetSequenceId() = 0;

    // Returns true if sequence should yield while running its current task.
    virtual bool ShouldYield() = 0;

    // Enables or disables further execution of tasks in this sequence.
    virtual void SetEnabled(bool enabled) = 0;

    // Schedule a task with provided sync token dependencies. The dependencies
    // are hints for sync token waits within the task, and can be ignored by the
    // implementation.
    virtual void ScheduleTask(base::OnceClosure task,
                              std::vector<SyncToken> sync_token_fences) = 0;

    // Continue running the current task after yielding execution.
    virtual void ContinueTask(base::OnceClosure task) = 0;
  };

  CommandBufferTaskExecutor(const GpuPreferences& gpu_preferences,
                            const GpuFeatureInfo& gpu_feature_info,
                            SyncPointManager* sync_point_manager,
                            MailboxManager* mailbox_manager,
                            scoped_refptr<gl::GLShareGroup> share_group,
                            gl::GLSurfaceFormat share_group_surface_format);

  // Always use virtualized GL contexts if this returns true.
  virtual bool ForceVirtualizedGLContexts() const = 0;

  // Creates a memory tracker for the context group if this returns true.
  virtual bool ShouldCreateMemoryTracker() const = 0;

  // Block thread when a WaitSyncToken command is encountered instead of calling
  // OnWaitSyncToken().
  virtual bool BlockThreadOnWaitSyncToken() const = 0;

  // Schedules |task| to run out of order with respect to other sequenced tasks.
  virtual void ScheduleOutOfOrderTask(base::OnceClosure task) = 0;

  // Schedules |task| to run at an appropriate time for performing delayed work.
  virtual void ScheduleDelayedWork(base::OnceClosure task) = 0;

  // Returns a new task execution sequence. Sequences should not outlive the
  // task executor.
  virtual std::unique_ptr<Sequence> CreateSequence() = 0;

  const GpuPreferences& gpu_preferences() const { return gpu_preferences_; }
  const GpuFeatureInfo& gpu_feature_info() const { return gpu_feature_info_; }
  gl::GLSurfaceFormat share_group_surface_format() const {
    return share_group_surface_format_;
  }
  SyncPointManager* sync_point_manager() const { return sync_point_manager_; }
  MailboxManager* mailbox_manager() const { return mailbox_manager_; }

  // Not const because these return inner pointers.
  gles2::ImageManager* image_manager() { return &image_manager_; }
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
  SharedImageManager* shared_image_manager() { return &shared_image_manager_; }

  // These methods construct accessed fields if not already initialized.
  scoped_refptr<gl::GLShareGroup> share_group();
  gles2::Outputter* outputter();
  gles2::ProgramCache* program_cache();

 protected:
  friend class base::RefCountedThreadSafe<CommandBufferTaskExecutor>;

  virtual ~CommandBufferTaskExecutor();

 private:
  const GpuPreferences gpu_preferences_;
  const GpuFeatureInfo gpu_feature_info_;
  std::unique_ptr<MailboxManager> owned_mailbox_manager_;
  SyncPointManager* sync_point_manager_;
  MailboxManager* mailbox_manager_;
  std::unique_ptr<gles2::Outputter> outputter_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  gl::GLSurfaceFormat share_group_surface_format_;
  std::unique_ptr<gles2::ProgramCache> program_cache_;
  gles2::ImageManager image_manager_;
  ServiceDiscardableManager discardable_manager_;
  PassthroughDiscardableManager passthrough_discardable_manager_;
  gles2::ShaderTranslatorCache shader_translator_cache_;
  gles2::FramebufferCompletenessCache framebuffer_completeness_cache_;
  SharedImageManager shared_image_manager_;

  // No-op default initialization is used in in-process mode.
  GpuProcessActivityFlags activity_flags_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferTaskExecutor);
};

}  // namespace gpu

#endif  // GPU_IPC_COMMAND_BUFFER_TASK_EXECUTOR_H_
