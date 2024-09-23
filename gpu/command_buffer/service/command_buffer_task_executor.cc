// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_buffer_task_executor.h"

#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/memory_program_cache.h"
#include "gpu/command_buffer/service/program_cache.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_share_group.h"

namespace gpu {

CommandBufferTaskExecutor::CommandBufferTaskExecutor(
    const GpuPreferences& gpu_preferences,
    const GpuFeatureInfo& gpu_feature_info,
    SyncPointManager* sync_point_manager,
    gl::GLSurfaceFormat share_group_surface_format,
    SharedImageManager* shared_image_manager,
    gles2::ProgramCache* program_cache)
    : gpu_preferences_(gpu_preferences),
      gpu_feature_info_(gpu_feature_info),
      sync_point_manager_(sync_point_manager),
      share_group_surface_format_(share_group_surface_format),
      program_cache_(program_cache),
      discardable_manager_(gpu_preferences_),
      passthrough_discardable_manager_(gpu_preferences_),
      shader_translator_cache_(gpu_preferences_),
      shared_image_manager_(shared_image_manager) {
  DCHECK(shared_image_manager_);
}

CommandBufferTaskExecutor::~CommandBufferTaskExecutor() = default;

gles2::Outputter* CommandBufferTaskExecutor::outputter() {
  if (!outputter_) {
    outputter_ =
        std::make_unique<gles2::TraceOutputter>("InProcessCommandBuffer Trace");
  }
  return outputter_.get();
}

gles2::ProgramCache* CommandBufferTaskExecutor::program_cache() {
  if (!program_cache_ &&
      gl::g_current_gl_driver->ext.b_GL_OES_get_program_binary &&
      !gpu_preferences().disable_gpu_program_cache) {
    bool disable_disk_cache =
        gpu_preferences_.disable_gpu_shader_disk_cache ||
        gpu_feature_info_.IsWorkaroundEnabled(gpu::DISABLE_PROGRAM_DISK_CACHE);
    owned_program_cache_ = std::make_unique<gles2::MemoryProgramCache>(
        gpu_preferences_.gpu_program_cache_size, disable_disk_cache,
        gpu_feature_info_.IsWorkaroundEnabled(
            gpu::DISABLE_PROGRAM_CACHING_FOR_TRANSFORM_FEEDBACK),
        &use_shader_cache_shm_count_);
    program_cache_ = owned_program_cache_.get();
  }
  return program_cache_;
}

}  // namespace gpu
