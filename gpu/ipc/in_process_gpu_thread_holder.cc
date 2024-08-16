// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/in_process_gpu_thread_holder.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_util.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

InProcessGpuThreadHolder::InProcessGpuThreadHolder()
    : base::Thread("GpuThread") {
  DCHECK(base::CommandLine::InitializedForCurrentProcess());
  auto* command_line = base::CommandLine::ForCurrentProcess();
  gpu_preferences_ = gles2::ParseGpuPreferences(command_line);

  gpu::GPUInfo gpu_info;
  gpu::CollectGraphicsInfoForTesting(&gpu_info);
  gpu_feature_info_ = gpu::ComputeGpuFeatureInfo(gpu_info, gpu_preferences_,
                                                 command_line, nullptr);

  Start();
}

InProcessGpuThreadHolder::~InProcessGpuThreadHolder() {
  // Ensure members created on GPU thread are destroyed there too.
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&InProcessGpuThreadHolder::DeleteOnGpuThread,
                                base::Unretained(this)));
  Stop();
}

GpuPreferences* InProcessGpuThreadHolder::GetGpuPreferences() {
  DCHECK(!task_executor_);
  return &gpu_preferences_;
}

GpuFeatureInfo* InProcessGpuThreadHolder::GetGpuFeatureInfo() {
  DCHECK(!task_executor_);
  return &gpu_feature_info_;
}

CommandBufferTaskExecutor* InProcessGpuThreadHolder::GetTaskExecutor() {
  if (!task_executor_) {
    base::WaitableEvent completion;
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&InProcessGpuThreadHolder::InitializeOnGpuThread,
                       base::Unretained(this), &completion));
    completion.Wait();
  }
  return task_executor_.get();
}

void InProcessGpuThreadHolder::InitializeOnGpuThread(
    base::WaitableEvent* completion) {
  sync_point_manager_ = std::make_unique<SyncPointManager>();
  scheduler_ = std::make_unique<Scheduler>(sync_point_manager_.get());
  shared_image_manager_ = std::make_unique<SharedImageManager>();

  bool use_passthrough_cmd_decoder =
      gpu_preferences_.use_passthrough_cmd_decoder &&
      gles2::PassthroughCommandDecoderSupported();

  share_group_ = new gl::GLShareGroup();
  surface_ =
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size());
  gl::GLContextAttribs attribs =
      gles2::GenerateGLContextAttribsForCompositor(use_passthrough_cmd_decoder);
  context_ =
      gl::init::CreateGLContext(share_group_.get(), surface_.get(), attribs);
  CHECK(context_->MakeCurrent(surface_.get()));
  GpuDriverBugWorkarounds gpu_driver_bug_workarounds(
      gpu_feature_info_.enabled_gpu_driver_bug_workarounds);

  bool use_virtualized_gl_context = false;
#if BUILDFLAG(IS_MAC)
  // Virtualize GpuPreference:::kLowPower contexts by default on OS X to prevent
  // performance regressions when enabling FCM. https://crbug.com/180463
  use_virtualized_gl_context = true;
#endif
  use_virtualized_gl_context |=
      gpu_driver_bug_workarounds.use_virtualized_gl_contexts;
  if (use_passthrough_cmd_decoder) {
    // Virtualized contexts don't work with passthrough command decoder.
    // See https://crbug.com/914976
    use_virtualized_gl_context = false;
  }
  if (use_virtualized_gl_context)
    share_group_->SetSharedContext(context_.get());

  context_state_ = base::MakeRefCounted<SharedContextState>(
      share_group_, surface_, context_, use_virtualized_gl_context,
      base::DoNothing(), gpu_preferences_.gr_context_type);
  auto feature_info = base::MakeRefCounted<gles2::FeatureInfo>(
      gpu_driver_bug_workarounds, gpu_feature_info_);
  context_state_->InitializeGL(gpu_preferences_, feature_info);
  context_state_->InitializeSkia(gpu_preferences_, gpu_driver_bug_workarounds);

  task_executor_ = std::make_unique<GpuInProcessThreadService>(
      this, task_runner(), scheduler_.get(), sync_point_manager_.get(),
      gl::GLSurfaceFormat(), gpu_feature_info_, gpu_preferences_,
      shared_image_manager_.get(), nullptr);

  completion->Signal();
}

void InProcessGpuThreadHolder::DeleteOnGpuThread() {
  task_executor_.reset();
  scheduler_.reset();
  sync_point_manager_.reset();
  shared_image_manager_.reset();

  context_state_.reset();
  context_.reset();
  surface_.reset();
  share_group_.reset();
}

scoped_refptr<SharedContextState>
InProcessGpuThreadHolder::GetSharedContextState() {
  DCHECK(context_state_);
  return context_state_;
}

scoped_refptr<gl::GLShareGroup> InProcessGpuThreadHolder::GetShareGroup() {
  if (!share_group_)
    share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
  return share_group_;
}

}  // namespace gpu
