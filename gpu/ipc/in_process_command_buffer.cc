// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/in_process_command_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/swap_buffers_flags.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_command_buffer_memory_tracker.h"
#include "gpu/command_buffer/service/gpu_fence_manager.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager_factory.h"
#include "gpu/command_buffer/service/memory_program_cache.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/raster_decoder.h"
#include "gpu/command_buffer/service/raster_decoder_context_state.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "gpu/ipc/in_process_gpu_thread_holder.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/create_gr_gl_interface.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/process/process_handle.h"
#endif

namespace gpu {

namespace {

base::AtomicSequenceNumber g_next_route_id;
base::AtomicSequenceNumber g_next_image_id;
base::AtomicSequenceNumber g_next_transfer_buffer_id;

CommandBufferId NextCommandBufferId() {
  return CommandBufferIdFromChannelAndRoute(kInProcessCommandBufferClientId,
                                            g_next_route_id.GetNext() + 1);
}

template <typename T>
base::OnceClosure WrapTaskWithResult(base::OnceCallback<T(void)> task,
                                     T* result,
                                     base::WaitableEvent* completion) {
  auto wrapper = [](base::OnceCallback<T(void)> task, T* result,
                    base::WaitableEvent* completion) {
    *result = std::move(task).Run();
    completion->Signal();
  };
  return base::BindOnce(wrapper, std::move(task), result, completion);
}

base::LazyInstance<InProcessGpuThreadHolder>::DestructorAtExit
    g_default_task_executer = LAZY_INSTANCE_INITIALIZER;

class ScopedEvent {
 public:
  explicit ScopedEvent(base::WaitableEvent* event) : event_(event) {}
  ~ScopedEvent() { event_->Signal(); }

 private:
  base::WaitableEvent* event_;
};

// If |task_executer| is passed in then it will be returned, otherwise a default
// task_executer will be constructed and returned.
scoped_refptr<CommandBufferTaskExecutor> MaybeGetDefaultTaskExecutor(
    scoped_refptr<CommandBufferTaskExecutor> task_executer) {
  if (task_executer)
    return task_executer;

  // Call base::ThreadTaskRunnerHandle::IsSet() to ensure that it is
  // instantiated before we create the GPU thread, otherwise shutdown order will
  // delete the ThreadTaskRunnerHandle before the GPU thread's message loop,
  // and when the message loop is shutdown, it will recreate
  // ThreadTaskRunnerHandle, which will re-add a new task to the, AtExitManager,
  // which causes a deadlock because it's already locked.
  base::ThreadTaskRunnerHandle::IsSet();
  return g_default_task_executer.Get().GetTaskExecutor();
}

}  // namespace

class InProcessCommandBuffer::SharedImageInterface
    : public gpu::SharedImageInterface {
 public:
  explicit SharedImageInterface(InProcessCommandBuffer* parent)
      : parent_(parent), command_buffer_id_(NextCommandBufferId()) {}

  ~SharedImageInterface() override = default;

  Mailbox CreateSharedImage(viz::ResourceFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            uint32_t usage) override {
    auto mailbox = Mailbox::Generate();
    {
      base::AutoLock lock(lock_);
      // Note: we enqueue the task under the lock to guarantee monotonicity of
      // the release ids as seen by the service. Unretained is safe because
      // InProcessCommandBuffer synchronizes with the GPU thread at destruction
      // time, cancelling tasks, before |this| is destroyed.
      parent_->ScheduleGpuTask(base::BindOnce(
          &InProcessCommandBuffer::CreateSharedImageOnGpuThread,
          parent_->gpu_thread_weak_ptr_factory_.GetWeakPtr(), mailbox, format,
          size, color_space, usage, MakeSyncToken(next_fence_sync_release_++)));
    }
    return mailbox;
  }

  void DestroySharedImage(const SyncToken& sync_token,
                          const Mailbox& mailbox) override {
    // Use sync token dependency to ensure that the destroy task does not run
    // before sync token is released.
    parent_->ScheduleGpuTask(
        base::BindOnce(&InProcessCommandBuffer::DestroySharedImageOnGpuThread,
                       parent_->gpu_thread_weak_ptr_factory_.GetWeakPtr(),
                       mailbox),
        {sync_token});
  }

  SyncToken GenUnverifiedSyncToken() override {
    base::AutoLock lock(lock_);
    return MakeSyncToken(next_fence_sync_release_ - 1);
  }

  CommandBufferId command_buffer_id() const { return command_buffer_id_; }

 private:
  SyncToken MakeSyncToken(uint64_t release_id) {
    return SyncToken(CommandBufferNamespace::IN_PROCESS, command_buffer_id_,
                     release_id);
  }

  InProcessCommandBuffer* const parent_;

  const CommandBufferId command_buffer_id_;

  // Accessed on any thread. release_id_lock_ protects access to
  // next_fence_sync_release_.
  base::Lock lock_;
  uint64_t next_fence_sync_release_ = 1;

  DISALLOW_COPY_AND_ASSIGN(SharedImageInterface);
};

InProcessCommandBuffer::InProcessCommandBuffer(
    scoped_refptr<CommandBufferTaskExecutor> task_executer)
    : command_buffer_id_(NextCommandBufferId()),
      flush_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      task_executor_(MaybeGetDefaultTaskExecutor(std::move(task_executer))),
      shared_image_interface_(new SharedImageInterface(this)),
      fence_sync_wait_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED),
      client_thread_weak_ptr_factory_(this),
      gpu_thread_weak_ptr_factory_(this) {
  // This binds the client sequence checker to the current sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  // Detach gpu sequence checker because we want to bind it to the gpu sequence,
  // and not the current (client) sequence except for webview (see Initialize).
  DETACH_FROM_SEQUENCE(gpu_sequence_checker_);
}

InProcessCommandBuffer::~InProcessCommandBuffer() {
  Destroy();
}

// static
void InProcessCommandBuffer::InitializeDefaultServiceForTesting(
    const GpuFeatureInfo& gpu_feature_info) {
  // Call base::ThreadTaskRunnerHandle::IsSet() to ensure that it is
  // instantiated before we create the GPU thread, otherwise shutdown order will
  // delete the ThreadTaskRunnerHandle before the GPU thread's message loop,
  // and when the message loop is shutdown, it will recreate
  // ThreadTaskRunnerHandle, which will re-add a new task to the, AtExitManager,
  // which causes a deadlock because it's already locked.
  base::ThreadTaskRunnerHandle::IsSet();
  g_default_task_executer.Get().SetGpuFeatureInfo(gpu_feature_info);
}

gpu::ServiceTransferCache* InProcessCommandBuffer::GetTransferCacheForTest()
    const {
  return static_cast<raster::RasterDecoder*>(decoder_.get())
      ->GetTransferCacheForTest();
}

int InProcessCommandBuffer::GetRasterDecoderIdForTest() const {
  return static_cast<raster::RasterDecoder*>(decoder_.get())
      ->DecoderIdForTest();
}

gpu::SharedImageInterface* InProcessCommandBuffer::GetSharedImageInterface()
    const {
  return shared_image_interface_.get();
}

bool InProcessCommandBuffer::MakeCurrent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  if (error::IsError(command_buffer_->GetState().error)) {
    DLOG(ERROR) << "MakeCurrent failed because context lost.";
    return false;
  }
  if (!decoder_->MakeCurrent()) {
    DLOG(ERROR) << "Context lost because MakeCurrent failed.";
    command_buffer_->SetParseError(error::kLostContext);
    return false;
  }
  return true;
}

gpu::ContextResult InProcessCommandBuffer::Initialize(
    scoped_refptr<gl::GLSurface> surface,
    bool is_offscreen,
    SurfaceHandle window,
    const ContextCreationAttribs& attribs,
    InProcessCommandBuffer* share_group,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    ImageFactory* image_factory,
    GpuChannelManagerDelegate* gpu_channel_manager_delegate,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gpu::raster::GrShaderCache* gr_shader_cache,
    GpuProcessActivityFlags* activity_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(!share_group ||
         task_executor_.get() == share_group->task_executor_.get());
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::Initialize")

  gpu_memory_buffer_manager_ = gpu_memory_buffer_manager;
  gpu_channel_manager_delegate_ = gpu_channel_manager_delegate;

  if (surface) {
    // If a surface is provided, we are running in a webview and should not have
    // a task runner.
    DCHECK(!task_runner);
    // GPU thread must be the same as client thread due to GLSurface not being
    // thread safe. This binds the gpu sequence checker to current sequence,
    // which is the client sequence. Otherwise, the gpu sequence checker will
    // be bound to the gpu thread's sequence when InitializeOnGpuThread runs.
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
    surface_ = surface;
  } else {
    DCHECK(task_runner);
    origin_task_runner_ = std::move(task_runner);
  }

  Capabilities capabilities;
  InitializeOnGpuThreadParams params(is_offscreen, window, attribs,
                                     &capabilities, share_group, image_factory,
                                     gr_shader_cache, activity_flags);

  base::OnceCallback<gpu::ContextResult(void)> init_task =
      base::BindOnce(&InProcessCommandBuffer::InitializeOnGpuThread,
                     base::Unretained(this), params);

  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  task_executor_->ScheduleOutOfOrderTask(
      WrapTaskWithResult(std::move(init_task), &result, &completion));
  completion.Wait();

  if (result == gpu::ContextResult::kSuccess)
    capabilities_ = capabilities;

  return result;
}

gpu::ContextResult InProcessCommandBuffer::InitializeOnGpuThread(
    const InitializeOnGpuThreadParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::InitializeOnGpuThread")

  // TODO(crbug.com/832243): This could use the TransferBufferManager owned by
  // |context_group_| instead.
  transfer_buffer_manager_ = std::make_unique<TransferBufferManager>(nullptr);

  GpuDriverBugWorkarounds workarounds(
      task_executor_->gpu_feature_info().enabled_gpu_driver_bug_workarounds);

  if (params.share_command_buffer) {
    context_group_ = params.share_command_buffer->context_group_;
  } else {
    std::unique_ptr<MemoryTracker> memory_tracker;
    // Android WebView won't have a memory tracker.
    if (task_executor_->ShouldCreateMemoryTracker()) {
      const uint64_t client_tracing_id =
          base::trace_event::MemoryDumpManager::GetInstance()
              ->GetTracingProcessId();
      memory_tracker = std::make_unique<GpuCommandBufferMemoryTracker>(
          kInProcessCommandBufferClientId, client_tracing_id,
          command_buffer_id_.GetUnsafeValue(), params.attribs.context_type,
          base::ThreadTaskRunnerHandle::Get());
    }

    auto feature_info = base::MakeRefCounted<gles2::FeatureInfo>(
        workarounds, task_executor_->gpu_feature_info());
    context_group_ = base::MakeRefCounted<gles2::ContextGroup>(
        task_executor_->gpu_preferences(),
        gles2::PassthroughCommandDecoderSupported(),
        task_executor_->mailbox_manager(), std::move(memory_tracker),
        task_executor_->shader_translator_cache(),
        task_executor_->framebuffer_completeness_cache(), feature_info,
        params.attribs.bind_generates_resource, task_executor_->image_manager(),
        params.image_factory, nullptr /* progress_reporter */,
        task_executor_->gpu_feature_info(),
        task_executor_->discardable_manager(),
        task_executor_->passthrough_discardable_manager(),
        task_executor_->shared_image_manager());
  }

#if defined(OS_MACOSX)
  // Virtualize PreferIntegratedGpu contexts by default on OS X to prevent
  // performance regressions when enabling FCM. https://crbug.com/180463
  use_virtualized_gl_context_ |=
      (params.attribs.gpu_preference == gl::PreferIntegratedGpu);
#endif

  use_virtualized_gl_context_ |= task_executor_->ForceVirtualizedGLContexts();

  // MailboxManagerSync synchronization correctness currently depends on having
  // only a single context. See https://crbug.com/510243 for details.
  use_virtualized_gl_context_ |= task_executor_->mailbox_manager()->UsesSync();

  use_virtualized_gl_context_ |=
      context_group_->feature_info()->workarounds().use_virtualized_gl_contexts;

  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");

  command_buffer_ = std::make_unique<CommandBufferService>(
      this, transfer_buffer_manager_.get());

  if (!surface_) {
    if (params.is_offscreen) {
      // TODO(crbug.com/832243): GLES2CommandBufferStub has additional logic for
      // offscreen surfaces that might be needed here.
      surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
      if (!surface_.get()) {
        DestroyOnGpuThread();
        LOG(ERROR) << "ContextResult::kFatalFailure: Failed to create surface.";
        return gpu::ContextResult::kFatalFailure;
      }
    } else {
      gl::GLSurfaceFormat surface_format;

#if defined(OS_ANDROID)
      // Handle Android low-bit-depth surface formats.
      if (params.attribs.red_size <= 5 && params.attribs.green_size <= 6 &&
          params.attribs.blue_size <= 5 && params.attribs.alpha_size == 0) {
        // We hit this code path when creating the onscreen render context
        // used for compositing on low-end Android devices.
        surface_format.SetRGB565();
        DVLOG(1) << __FUNCTION__ << ": Choosing RGB565 mode.";
      }

      if (!surface_format.IsCompatible(
              task_executor_->share_group_surface_format())) {
        use_virtualized_gl_context_ = false;
      }
#endif

      switch (params.attribs.color_space) {
        case COLOR_SPACE_UNSPECIFIED:
          surface_format.SetColorSpace(
              gl::GLSurfaceFormat::COLOR_SPACE_UNSPECIFIED);
          break;
        case COLOR_SPACE_SRGB:
          surface_format.SetColorSpace(gl::GLSurfaceFormat::COLOR_SPACE_SRGB);
          break;
        case COLOR_SPACE_DISPLAY_P3:
          surface_format.SetColorSpace(
              gl::GLSurfaceFormat::COLOR_SPACE_DISPLAY_P3);
          break;
      }
      surface_ = ImageTransportSurface::CreateNativeSurface(
          gpu_thread_weak_ptr_factory_.GetWeakPtr(), params.window,
          surface_format);
      if (!surface_ || !surface_->Initialize(surface_format)) {
        DestroyOnGpuThread();
        LOG(ERROR)
            << "ContextResult::kSurfaceFailure: Failed to create surface.";
        return gpu::ContextResult::kSurfaceFailure;
      }
      if (params.attribs.enable_swap_timestamps_if_supported &&
          surface_->SupportsSwapTimestamps())
        surface_->SetEnableSwapTimestamps();
    }
  }

  task_sequence_ = task_executor_->CreateSequence();

  sync_point_client_state_ =
      task_executor_->sync_point_manager()->CreateSyncPointClientState(
          GetNamespaceID(), GetCommandBufferID(),
          task_sequence_->GetSequenceId());
  // Make the SharedImageInterface use the same sequence as the command buffer,
  // it's necessary for WebView because of the blocking behavior.
  // TODO(piman): see if it's worth using a different sequence for non-WebView.
  shared_image_client_state_ =
      task_executor_->sync_point_manager()->CreateSyncPointClientState(
          CommandBufferNamespace::IN_PROCESS,
          shared_image_interface_->command_buffer_id(),
          task_sequence_->GetSequenceId());

  if (context_group_->use_passthrough_cmd_decoder()) {
    // When using the passthrough command decoder, only share with other
    // contexts in the explicitly requested share group.
    if (params.share_command_buffer) {
      gl_share_group_ = params.share_command_buffer->gl_share_group_;
    } else {
      gl_share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
    }
  } else {
    // When using the validating command decoder, always use the global share
    // group.
    gl_share_group_ = task_executor_->share_group();
  }

  if (params.attribs.context_type == CONTEXT_TYPE_WEBGPU) {
    if (!task_executor_->gpu_preferences().enable_webgpu) {
      DLOG(ERROR) << "ContextResult::kFatalFailure: WebGPU not enabled";
      return gpu::ContextResult::kFatalFailure;
    }
    decoder_.reset(webgpu::WebGPUDecoder::Create(this, command_buffer_.get(),
                                                 task_executor_->outputter()));
  } else {
    // TODO(khushalsagar): A lot of this initialization code is duplicated in
    // GpuChannelManager. Pull it into a common util method.
    scoped_refptr<gl::GLContext> real_context =
        use_virtualized_gl_context_
            ? gl_share_group_->GetSharedContext(surface_.get())
            : nullptr;
    if (!real_context) {
      real_context = gl::init::CreateGLContext(
          gl_share_group_.get(), surface_.get(),
          GenerateGLContextAttribs(params.attribs, context_group_.get()));
      if (!real_context) {
        // TODO(piman): This might not be fatal, we could recurse into
        // CreateGLContext to get more info, tho it should be exceedingly
        // rare and may not be recoverable anyway.
        DestroyOnGpuThread();
        LOG(ERROR) << "ContextResult::kFatalFailure: "
                      "Failed to create shared context for virtualization.";
        return gpu::ContextResult::kFatalFailure;
      }
      // Ensure that context creation did not lose track of the intended share
      // group.
      DCHECK(real_context->share_group() == gl_share_group_.get());
      task_executor_->gpu_feature_info().ApplyToGLContext(real_context.get());

      if (use_virtualized_gl_context_)
        gl_share_group_->SetSharedContext(surface_.get(), real_context.get());
    }

    if (!real_context->MakeCurrent(surface_.get())) {
      LOG(ERROR)
          << "ContextResult::kTransientFailure, failed to make context current";
      DestroyOnGpuThread();
      return ContextResult::kTransientFailure;
    }

    bool use_passthrough_cmd_decoder =
        task_executor_->gpu_preferences().use_passthrough_cmd_decoder &&
        gles2::PassthroughCommandDecoderSupported();
    if (!use_passthrough_cmd_decoder &&
        params.attribs.enable_raster_interface &&
        !params.attribs.enable_gles2_interface) {
      context_state_ = base::MakeRefCounted<raster::RasterDecoderContextState>(
          gl_share_group_, surface_, real_context, use_virtualized_gl_context_);
      gr_shader_cache_ = params.gr_shader_cache;
      context_state_->InitializeGrContext(workarounds, params.gr_shader_cache,
                                          params.activity_flags);

      if (base::ThreadTaskRunnerHandle::IsSet()) {
        gr_cache_controller_.emplace(context_state_.get(),
                                     base::ThreadTaskRunnerHandle::Get());
      }

      decoder_.reset(raster::RasterDecoder::Create(
          this, command_buffer_.get(), task_executor_->outputter(),
          context_group_.get(), context_state_));
    } else {
      decoder_.reset(gles2::GLES2Decoder::Create(this, command_buffer_.get(),
                                                 task_executor_->outputter(),
                                                 context_group_.get()));
    }

    if (use_virtualized_gl_context_) {
      context_ = base::MakeRefCounted<GLContextVirtual>(
          gl_share_group_.get(), real_context.get(), decoder_->AsWeakPtr());
      if (!context_->Initialize(
              surface_.get(),
              GenerateGLContextAttribs(params.attribs, context_group_.get()))) {
        // TODO(piman): This might not be fatal, we could recurse into
        // CreateGLContext to get more info, tho it should be exceedingly
        // rare and may not be recoverable anyway.
        DestroyOnGpuThread();
        LOG(ERROR) << "ContextResult::kFatalFailure: "
                      "Failed to initialize virtual GL context.";
        return gpu::ContextResult::kFatalFailure;
      }

      if (!context_->MakeCurrent(surface_.get())) {
        DestroyOnGpuThread();
        // The caller should retry making a context, but this one won't work.
        LOG(ERROR) << "ContextResult::kTransientFailure: "
                      "Could not make context current.";
        return gpu::ContextResult::kTransientFailure;
      }

      context_->SetGLStateRestorer(
          new GLStateRestorerImpl(decoder_->AsWeakPtr()));
    } else {
      context_ = real_context;
      DCHECK(context_->IsCurrent(surface_.get()));
    }

    if (!context_group_->has_program_cache() &&
        !context_group_->feature_info()->workarounds().disable_program_cache) {
      context_group_->set_program_cache(task_executor_->program_cache());
    }
  }

  gles2::DisallowedFeatures disallowed_features;
  auto result = decoder_->Initialize(surface_, context_, params.is_offscreen,
                                     disallowed_features, params.attribs);
  if (result != gpu::ContextResult::kSuccess) {
    DestroyOnGpuThread();
    DLOG(ERROR) << "Failed to initialize decoder.";
    return result;
  }

  if (task_executor_->gpu_preferences().enable_gpu_service_logging)
    decoder_->SetLogCommands(true);

  if (context_ && use_virtualized_gl_context_) {
    // If virtualized GL contexts are in use, then real GL context state
    // is in an indeterminate state, since the GLStateRestorer was not
    // initialized at the time the GLContextVirtual was made current. In
    // the case that this command decoder is the next one to be
    // processed, force a "full virtual" MakeCurrent to be performed.
    context_->ForceReleaseVirtuallyCurrent();
    if (!context_->MakeCurrent(surface_.get())) {
      DestroyOnGpuThread();
      LOG(ERROR) << "ContextResult::kTransientFailure: "
                    "Failed to make context current after initialization.";
      return gpu::ContextResult::kTransientFailure;
    }
  }

  *params.capabilities = decoder_->GetCapabilities();

  image_factory_ = params.image_factory;

  return gpu::ContextResult::kSuccess;
}

void InProcessCommandBuffer::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::Destroy");

  client_thread_weak_ptr_factory_.InvalidateWeakPtrs();
  gpu_control_client_ = nullptr;
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool result = false;
  base::OnceCallback<bool(void)> destroy_task = base::BindOnce(
      &InProcessCommandBuffer::DestroyOnGpuThread, base::Unretained(this));
  task_executor_->ScheduleOutOfOrderTask(
      WrapTaskWithResult(std::move(destroy_task), &result, &completion));
  completion.Wait();
}

bool InProcessCommandBuffer::DestroyOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::DestroyOnGpuThread");

  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");
  gpu_thread_weak_ptr_factory_.InvalidateWeakPtrs();
  // Clean up GL resources if possible.
  bool have_context = context_.get() && context_->MakeCurrent(surface_.get());
  if (shared_image_factory_)
    shared_image_factory_->DestroyAllSharedImages(have_context);

  // Prepare to destroy the surface while the context is still current, because
  // some surface destructors make GL calls.
  if (surface_)
    surface_->PrepareToDestroy(have_context);

  if (decoder_) {
    gr_cache_controller_.reset();
    decoder_->Destroy(have_context);
    decoder_.reset();
  }
  command_buffer_.reset();
  transfer_buffer_manager_.reset();
  surface_ = nullptr;

  context_ = nullptr;
  if (sync_point_client_state_) {
    sync_point_client_state_->Destroy();
    sync_point_client_state_ = nullptr;
  }
  if (shared_image_client_state_) {
    shared_image_client_state_->Destroy();
    shared_image_client_state_ = nullptr;
  }
  gl_share_group_ = nullptr;
  context_group_ = nullptr;
  task_sequence_ = nullptr;
  context_state_ = nullptr;
  return true;
}

CommandBufferServiceClient::CommandBatchProcessedResult
InProcessCommandBuffer::OnCommandBatchProcessed() {
  return task_sequence_->ShouldYield() ? kPauseExecution : kContinueExecution;
}

void InProcessCommandBuffer::OnParseError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  PostOrRunClientCallback(
      base::BindOnce(&InProcessCommandBuffer::OnContextLost,
                     client_thread_weak_ptr_factory_.GetWeakPtr()));
}

void InProcessCommandBuffer::OnContextLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

#if DCHECK_IS_ON()
  // This method shouldn't be called more than once.
  DCHECK(!context_lost_);
  context_lost_ = true;
#endif

  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlLostContext();
}

void InProcessCommandBuffer::RunTaskOnGpuThread(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");
  std::move(task).Run();
}

void InProcessCommandBuffer::ScheduleGpuTask(
    base::OnceClosure task,
    std::vector<SyncToken> sync_token_fences) {
  base::OnceClosure gpu_task = base::BindOnce(
      &InProcessCommandBuffer::RunTaskOnGpuThread,
      gpu_thread_weak_ptr_factory_.GetWeakPtr(), std::move(task));
  task_sequence_->ScheduleTask(std::move(gpu_task),
                               std::move(sync_token_fences));
}

void InProcessCommandBuffer::ContinueGpuTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  base::OnceClosure gpu_task = base::BindOnce(
      &InProcessCommandBuffer::RunTaskOnGpuThread,
      gpu_thread_weak_ptr_factory_.GetWeakPtr(), std::move(task));
  task_sequence_->ContinueTask(std::move(gpu_task));
}

CommandBuffer::State InProcessCommandBuffer::GetLastState() {
  base::AutoLock lock(last_state_lock_);
  return last_state_;
}

void InProcessCommandBuffer::UpdateLastStateOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  base::AutoLock lock(last_state_lock_);
  command_buffer_->UpdateState();
  State state = command_buffer_->GetState();
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
}

bool InProcessCommandBuffer::HasUnprocessedCommandsOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (command_buffer_) {
    CommandBuffer::State state = command_buffer_->GetState();
    return command_buffer_->put_offset() != state.get_offset &&
           !error::IsError(state.error);
  }
  return false;
}

void InProcessCommandBuffer::FlushOnGpuThread(int32_t put_offset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  TRACE_EVENT1("gpu", "InProcessCommandBuffer::FlushOnGpuThread", "put_offset",
               put_offset);

  ScopedEvent handle_flush(&flush_event_);

  if (!MakeCurrent())
    return;

  {
    base::Optional<raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache_)
      cache_use.emplace(gr_shader_cache_, kInProcessCommandBufferClientId);
    command_buffer_->Flush(put_offset, decoder_.get());
  }
  // Update state before signaling the flush event.
  UpdateLastStateOnGpuThread();

  bool has_unprocessed_commands = HasUnprocessedCommandsOnGpuThread();

  if (!command_buffer_->scheduled() || has_unprocessed_commands) {
    DCHECK(!task_executor_->BlockThreadOnWaitSyncToken());
    ContinueGpuTask(base::BindOnce(&InProcessCommandBuffer::FlushOnGpuThread,
                                   gpu_thread_weak_ptr_factory_.GetWeakPtr(),
                                   put_offset));
  }

  // If we've processed all pending commands but still have pending queries,
  // pump idle work until the query is passed.
  if (!has_unprocessed_commands &&
      (decoder_->HasMoreIdleWork() || decoder_->HasPendingQueries())) {
    ScheduleDelayedWorkOnGpuThread();
  }
}

void InProcessCommandBuffer::PerformDelayedWorkOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  delayed_work_pending_ = false;
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");
  if (MakeCurrent()) {
    decoder_->PerformIdleWork();
    decoder_->ProcessPendingQueries(false);
    if (decoder_->HasMoreIdleWork() || decoder_->HasPendingQueries()) {
      ScheduleDelayedWorkOnGpuThread();
    }
  }
}

void InProcessCommandBuffer::ScheduleDelayedWorkOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (delayed_work_pending_)
    return;
  delayed_work_pending_ = true;
  task_executor_->ScheduleDelayedWork(
      base::Bind(&InProcessCommandBuffer::PerformDelayedWorkOnGpuThread,
                 gpu_thread_weak_ptr_factory_.GetWeakPtr()));
}

void InProcessCommandBuffer::Flush(int32_t put_offset) {
  if (GetLastState().error != error::kNoError)
    return;

  if (last_put_offset_ == put_offset)
    return;

  TRACE_EVENT1("gpu", "InProcessCommandBuffer::Flush", "put_offset",
               put_offset);

  last_put_offset_ = put_offset;
  flushed_fence_sync_release_ = next_fence_sync_release_ - 1;

  std::vector<SyncToken> sync_token_fences;
  next_flush_sync_token_fences_.swap(sync_token_fences);

  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::FlushOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), put_offset),
      std::move(sync_token_fences));
}

void InProcessCommandBuffer::OrderingBarrier(int32_t put_offset) {
  Flush(put_offset);
}

CommandBuffer::State InProcessCommandBuffer::WaitForTokenInRange(int32_t start,
                                                                 int32_t end) {
  TRACE_EVENT2("gpu", "InProcessCommandBuffer::WaitForTokenInRange", "start",
               start, "end", end);

  State last_state = GetLastState();
  while (!InRange(start, end, last_state.token) &&
         last_state.error == error::kNoError) {
    flush_event_.Wait();
    last_state = GetLastState();
  }
  return last_state;
}

CommandBuffer::State InProcessCommandBuffer::WaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end) {
  TRACE_EVENT2("gpu", "InProcessCommandBuffer::WaitForGetOffsetInRange",
               "start", start, "end", end);

  State last_state = GetLastState();
  while (((set_get_buffer_count != last_state.set_get_buffer_count) ||
          !InRange(start, end, last_state.get_offset)) &&
         last_state.error == error::kNoError) {
    flush_event_.Wait();
    last_state = GetLastState();
  }
  return last_state;
}

void InProcessCommandBuffer::SetGetBuffer(int32_t shm_id) {
  if (GetLastState().error != error::kNoError)
    return;

  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  ScheduleGpuTask(base::BindOnce(
      &InProcessCommandBuffer::SetGetBufferOnGpuThread,
      gpu_thread_weak_ptr_factory_.GetWeakPtr(), shm_id, &completion));
  completion.Wait();

  last_put_offset_ = 0;
}

void InProcessCommandBuffer::SetGetBufferOnGpuThread(
    int32_t shm_id,
    base::WaitableEvent* completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  command_buffer_->SetGetBuffer(shm_id);
  UpdateLastStateOnGpuThread();
  completion->Signal();
}

scoped_refptr<Buffer> InProcessCommandBuffer::CreateTransferBuffer(
    size_t size,
    int32_t* id) {
  scoped_refptr<Buffer> buffer = MakeMemoryBuffer(size);
  *id = g_next_transfer_buffer_id.GetNext() + 1;
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::RegisterTransferBufferOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), *id, buffer));
  return buffer;
}

void InProcessCommandBuffer::RegisterTransferBufferOnGpuThread(
    int32_t id,
    scoped_refptr<Buffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  command_buffer_->RegisterTransferBuffer(id, std::move(buffer));
}

void InProcessCommandBuffer::DestroyTransferBuffer(int32_t id) {
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::DestroyTransferBufferOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), id));
}

void InProcessCommandBuffer::DestroyTransferBufferOnGpuThread(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  command_buffer_->DestroyTransferBuffer(id);
}

void InProcessCommandBuffer::SetGpuControlClient(GpuControlClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  gpu_control_client_ = client;
}

const Capabilities& InProcessCommandBuffer::GetCapabilities() const {
  return capabilities_;
}

const GpuFeatureInfo& InProcessCommandBuffer::GetGpuFeatureInfo() const {
  return task_executor_->gpu_feature_info();
}

int32_t InProcessCommandBuffer::CreateImage(ClientBuffer buffer,
                                            size_t width,
                                            size_t height) {
  DCHECK(gpu_memory_buffer_manager_);
  gfx::GpuMemoryBuffer* gpu_memory_buffer =
      reinterpret_cast<gfx::GpuMemoryBuffer*>(buffer);
  DCHECK(gpu_memory_buffer);

  int32_t new_id = g_next_image_id.GetNext() + 1;

  DCHECK(IsImageFromGpuMemoryBufferFormatSupported(
      gpu_memory_buffer->GetFormat(), capabilities_));

  // This handle is owned by the GPU thread and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // queuing of the CreateImage task below.
  gfx::GpuMemoryBufferHandle handle = gpu_memory_buffer->CloneHandle();
  bool requires_sync_point = handle.type == gfx::IO_SURFACE_BUFFER;

  uint64_t fence_sync = 0;
  if (requires_sync_point) {
    fence_sync = GenerateFenceSyncRelease();

    // Previous fence syncs should be flushed already.
    DCHECK_EQ(fence_sync - 1, flushed_fence_sync_release_);
  }

  ScheduleGpuTask(base::BindOnce(
      &InProcessCommandBuffer::CreateImageOnGpuThread,
      gpu_thread_weak_ptr_factory_.GetWeakPtr(), new_id, std::move(handle),
      gfx::Size(base::checked_cast<int>(width),
                base::checked_cast<int>(height)),
      gpu_memory_buffer->GetFormat(), fence_sync));

  if (fence_sync) {
    flushed_fence_sync_release_ = fence_sync;
    SyncToken sync_token(GetNamespaceID(), GetCommandBufferID(), fence_sync);
    sync_token.SetVerifyFlush();
    gpu_memory_buffer_manager_->SetDestructionSyncToken(gpu_memory_buffer,
                                                        sync_token);
  }

  return new_id;
}

void InProcessCommandBuffer::CreateImageOnGpuThread(
    int32_t id,
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint64_t fence_sync) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  gles2::ImageManager* image_manager = task_executor_->image_manager();
  DCHECK(image_manager);
  if (image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image already exists with same ID.";
    return;
  }

  unsigned internalformat = gpu::InternalFormatForGpuMemoryBufferFormat(format);

  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      if (!base::IsValueInRangeForNumericType<size_t>(handle.stride)) {
        LOG(ERROR) << "Invalid stride for image.";
        return;
      }
      scoped_refptr<gl::GLImageSharedMemory> image(
          new gl::GLImageSharedMemory(size, internalformat));
      if (!image->Initialize(handle.region, handle.id, format, handle.offset,
                             handle.stride)) {
        LOG(ERROR) << "Failed to initialize image.";
        return;
      }

      image_manager->AddImage(image.get(), id);
      break;
    }
    default: {
      if (!image_factory_) {
        LOG(ERROR) << "Image factory missing but required by buffer type.";
        return;
      }

      scoped_refptr<gl::GLImage> image =
          image_factory_->CreateImageForGpuMemoryBuffer(
              std::move(handle), size, format, internalformat,
              kInProcessCommandBufferClientId, kNullSurfaceHandle);
      if (!image.get()) {
        LOG(ERROR) << "Failed to create image for buffer.";
        return;
      }

      image_manager->AddImage(image.get(), id);
      break;
    }
  }

  if (fence_sync)
    sync_point_client_state_->ReleaseFenceSync(fence_sync);
}

void InProcessCommandBuffer::DestroyImage(int32_t id) {
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::DestroyImageOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), id));
}

void InProcessCommandBuffer::DestroyImageOnGpuThread(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  gles2::ImageManager* image_manager = task_executor_->image_manager();
  DCHECK(image_manager);
  if (!image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image with ID doesn't exist.";
    return;
  }

  image_manager->RemoveImage(id);
}

void InProcessCommandBuffer::OnConsoleMessage(int32_t id,
                                              const std::string& message) {
  // TODO(piman): implement this.
}

void InProcessCommandBuffer::CacheShader(const std::string& key,
                                         const std::string& shader) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (gpu_channel_manager_delegate_)
    gpu_channel_manager_delegate_->StoreShaderToDisk(
        kInProcessCommandBufferClientId, key, shader);
}

void InProcessCommandBuffer::OnFenceSyncRelease(uint64_t release) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  SyncToken sync_token(GetNamespaceID(), GetCommandBufferID(), release);
  context_group_->mailbox_manager()->PushTextureUpdates(sync_token);
  sync_point_client_state_->ReleaseFenceSync(release);
}

// TODO(sunnyps): Remove the wait command once all sync tokens are passed as
// task dependencies.
bool InProcessCommandBuffer::OnWaitSyncToken(const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  DCHECK(!waiting_for_sync_point_);
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::OnWaitSyncToken");

  SyncPointManager* sync_point_manager = task_executor_->sync_point_manager();
  DCHECK(sync_point_manager);

  MailboxManager* mailbox_manager = context_group_->mailbox_manager();
  DCHECK(mailbox_manager);

  if (task_executor_->BlockThreadOnWaitSyncToken()) {
    // Wait if sync point wait is valid.
    if (sync_point_client_state_->Wait(
            sync_token,
            base::Bind(&base::WaitableEvent::Signal,
                       base::Unretained(&fence_sync_wait_event_)))) {
      fence_sync_wait_event_.Wait();
    }

    mailbox_manager->PullTextureUpdates(sync_token);
    return false;
  }

  waiting_for_sync_point_ = sync_point_client_state_->Wait(
      sync_token,
      base::Bind(&InProcessCommandBuffer::OnWaitSyncTokenCompleted,
                 gpu_thread_weak_ptr_factory_.GetWeakPtr(), sync_token));
  if (!waiting_for_sync_point_) {
    mailbox_manager->PullTextureUpdates(sync_token);
    return false;
  }

  command_buffer_->SetScheduled(false);
  task_sequence_->SetEnabled(false);
  return true;
}

void InProcessCommandBuffer::OnWaitSyncTokenCompleted(
    const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  DCHECK(waiting_for_sync_point_);
  context_group_->mailbox_manager()->PullTextureUpdates(sync_token);
  waiting_for_sync_point_ = false;
  command_buffer_->SetScheduled(true);
  task_sequence_->SetEnabled(true);
}

void InProcessCommandBuffer::OnDescheduleUntilFinished() {
  NOTREACHED();
}

void InProcessCommandBuffer::OnRescheduleAfterFinished() {
  NOTREACHED();
}

void InProcessCommandBuffer::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  pending_swap_completed_params_.push_back({swap_id, flags});
  pending_presented_params_.push_back({swap_id, flags});
}

void InProcessCommandBuffer::ScheduleGrContextCleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (gr_cache_controller_)
    gr_cache_controller_->ScheduleGrContextCleanup();
}

void InProcessCommandBuffer::PostOrRunClientCallback(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (origin_task_runner_ && !origin_task_runner_->BelongsToCurrentThread())
    origin_task_runner_->PostTask(FROM_HERE, std::move(callback));
  else
    std::move(callback).Run();
}

base::OnceClosure InProcessCommandBuffer::WrapClientCallback(
    base::OnceClosure callback) {
  return base::BindOnce(&InProcessCommandBuffer::PostOrRunClientCallback,
                        gpu_thread_weak_ptr_factory_.GetWeakPtr(),
                        std::move(callback));
}

void InProcessCommandBuffer::SignalSyncToken(const SyncToken& sync_token,
                                             base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::SignalSyncTokenOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), sync_token,
                     std::move(callback)));
}

void InProcessCommandBuffer::SignalSyncTokenOnGpuThread(
    const SyncToken& sync_token,
    base::OnceClosure callback) {
  base::RepeatingClosure maybe_pass_callback =
      base::AdaptCallbackForRepeating(WrapClientCallback(std::move(callback)));
  if (!sync_point_client_state_->Wait(sync_token, maybe_pass_callback)) {
    maybe_pass_callback.Run();
  }
}

void InProcessCommandBuffer::SignalQuery(unsigned query_id,
                                         base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::SignalQueryOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), query_id,
                     std::move(callback)));
}

void InProcessCommandBuffer::SignalQueryOnGpuThread(
    unsigned query_id,
    base::OnceClosure callback) {
  decoder_->SetQueryCallback(query_id, WrapClientCallback(std::move(callback)));
}

void InProcessCommandBuffer::CreateGpuFence(uint32_t gpu_fence_id,
                                            ClientGpuFence source) {
  // Pass a cloned handle to the GPU process since the source ClientGpuFence
  // may go out of scope before the queued task runs.
  gfx::GpuFence* gpu_fence = gfx::GpuFence::FromClientGpuFence(source);
  gfx::GpuFenceHandle handle =
      gfx::CloneHandleForIPC(gpu_fence->GetGpuFenceHandle());

  ScheduleGpuTask(base::BindOnce(
      &InProcessCommandBuffer::CreateGpuFenceOnGpuThread,
      gpu_thread_weak_ptr_factory_.GetWeakPtr(), gpu_fence_id, handle));
}

void InProcessCommandBuffer::CreateGpuFenceOnGpuThread(
    uint32_t gpu_fence_id,
    const gfx::GpuFenceHandle& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!GetFeatureInfo()->feature_flags().chromium_gpu_fence) {
    DLOG(ERROR) << "CHROMIUM_gpu_fence unavailable";
    command_buffer_->SetParseError(error::kLostContext);
    return;
  }

  gles2::GpuFenceManager* gpu_fence_manager = decoder_->GetGpuFenceManager();
  DCHECK(gpu_fence_manager);

  if (gpu_fence_manager->CreateGpuFenceFromHandle(gpu_fence_id, handle))
    return;

  // The insertion failed. This shouldn't happen, force context loss to avoid
  // inconsistent state.
  command_buffer_->SetParseError(error::kLostContext);
}

void InProcessCommandBuffer::GetGpuFence(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::GetGpuFenceOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), gpu_fence_id,
                     std::move(callback)));
}

void InProcessCommandBuffer::GetGpuFenceOnGpuThread(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!GetFeatureInfo()->feature_flags().chromium_gpu_fence) {
    DLOG(ERROR) << "CHROMIUM_gpu_fence unavailable";
    command_buffer_->SetParseError(error::kLostContext);
    return;
  }

  gles2::GpuFenceManager* manager = decoder_->GetGpuFenceManager();
  DCHECK(manager);

  std::unique_ptr<gfx::GpuFence> gpu_fence;
  if (manager->IsValidGpuFence(gpu_fence_id)) {
    gpu_fence = manager->GetGpuFence(gpu_fence_id);
  } else {
    // Retrieval failed. This shouldn't happen, force context loss to avoid
    // inconsistent state.
    DLOG(ERROR) << "GpuFence not found";
    command_buffer_->SetParseError(error::kLostContext);
  }

  PostOrRunClientCallback(
      base::BindOnce(std::move(callback), std::move(gpu_fence)));
}

void InProcessCommandBuffer::CreateSharedImageOnGpuThread(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!MakeCurrent())
    return;
  if (!shared_image_factory_) {
    shared_image_factory_ = std::make_unique<SharedImageFactory>(
        GetGpuPreferences(), context_group_->feature_info()->workarounds(),
        GetGpuFeatureInfo(), context_state_.get(),
        context_group_->mailbox_manager(),
        context_group_->shared_image_manager(), image_factory_, nullptr);
  }
  if (!shared_image_factory_->CreateSharedImage(mailbox, format, size,
                                                color_space, usage)) {
    // Signal errors by losing the command buffer.
    command_buffer_->SetParseError(error::kLostContext);
    return;
  }
  context_group_->mailbox_manager()->PushTextureUpdates(sync_token);
  shared_image_client_state_->ReleaseFenceSync(sync_token.release_count());
}

void InProcessCommandBuffer::DestroySharedImageOnGpuThread(
    const Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!MakeCurrent())
    return;
  if (!shared_image_factory_ ||
      !shared_image_factory_->DestroySharedImage(mailbox)) {
    // Signal errors by losing the command buffer.
    command_buffer_->SetParseError(error::kLostContext);
  }
}

void InProcessCommandBuffer::SetLock(base::Lock*) {
  // No support for using on multiple threads.
  NOTREACHED();
}

void InProcessCommandBuffer::EnsureWorkVisible() {
  // This is only relevant for out-of-process command buffers.
}

CommandBufferNamespace InProcessCommandBuffer::GetNamespaceID() const {
  return CommandBufferNamespace::IN_PROCESS;
}

CommandBufferId InProcessCommandBuffer::GetCommandBufferID() const {
  return command_buffer_id_;
}

void InProcessCommandBuffer::FlushPendingWork() {
  // This is only relevant for out-of-process command buffers.
}

uint64_t InProcessCommandBuffer::GenerateFenceSyncRelease() {
  return next_fence_sync_release_++;
}

bool InProcessCommandBuffer::IsFenceSyncReleased(uint64_t release) {
  return release <= GetLastState().release_count;
}

void InProcessCommandBuffer::WaitSyncTokenHint(const SyncToken& sync_token) {
  next_flush_sync_token_fences_.push_back(sync_token);
}

bool InProcessCommandBuffer::CanWaitUnverifiedSyncToken(
    const SyncToken& sync_token) {
  return sync_token.namespace_id() == GetNamespaceID();
}

#if defined(OS_WIN)
void InProcessCommandBuffer::DidCreateAcceleratedSurfaceChildWindow(
    SurfaceHandle parent_window,
    SurfaceHandle child_window) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  // In the browser process call ::SetParent() directly.
  if (!gpu_channel_manager_delegate_) {
    ::SetParent(child_window, parent_window);
    // Move D3D window behind Chrome's window to avoid losing some messages.
    ::SetWindowPos(child_window, HWND_BOTTOM, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE);
    return;
  }

  // In the GPU process forward the request back to the browser process.
  gpu_channel_manager_delegate_->SendCreatedChildWindow(parent_window,
                                                        child_window);
}
#endif

void InProcessCommandBuffer::DidSwapBuffersComplete(
    SwapBuffersCompleteParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  params.swap_response.swap_id = pending_swap_completed_params_.front().swap_id;
  pending_swap_completed_params_.pop_front();

  PostOrRunClientCallback(base::BindOnce(
      &InProcessCommandBuffer::DidSwapBuffersCompleteOnOriginThread,
      client_thread_weak_ptr_factory_.GetWeakPtr(), base::Passed(&params)));
}

const gles2::FeatureInfo* InProcessCommandBuffer::GetFeatureInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  return context_group_->feature_info();
}

const GpuPreferences& InProcessCommandBuffer::GetGpuPreferences() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  return context_group_->gpu_preferences();
}

void InProcessCommandBuffer::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  SwapBufferParams params = pending_presented_params_.front();
  pending_presented_params_.pop_front();

  PostOrRunClientCallback(
      base::BindOnce(&InProcessCommandBuffer::BufferPresentedOnOriginThread,
                     client_thread_weak_ptr_factory_.GetWeakPtr(),
                     params.swap_id, params.flags, feedback));
}

void InProcessCommandBuffer::AddFilter(IPC::MessageFilter* message_filter) {
  NOTREACHED();
}

int32_t InProcessCommandBuffer::GetRouteID() const {
  NOTREACHED();
  return 0;
}

void InProcessCommandBuffer::DidSwapBuffersCompleteOnOriginThread(
    SwapBuffersCompleteParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlSwapBuffersCompleted(params);
}

void InProcessCommandBuffer::BufferPresentedOnOriginThread(
    uint64_t swap_id,
    uint32_t flags,
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  if (gpu_control_client_)
    gpu_control_client_->OnSwapBufferPresented(swap_id, feedback);
  if (flags & gpu::SwapBuffersFlags::kPresentationFeedback ||
      (flags & gpu::SwapBuffersFlags::kVSyncParams &&
       feedback.flags & gfx::PresentationFeedback::kVSync)) {
    if (update_vsync_parameters_completion_callback_ &&
        feedback.timestamp != base::TimeTicks())
      update_vsync_parameters_completion_callback_.Run(feedback.timestamp,
                                                       feedback.interval);
  }
}

void InProcessCommandBuffer::SetUpdateVSyncParametersCallback(
    const UpdateVSyncParametersCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  update_vsync_parameters_completion_callback_ = callback;
}

}  // namespace gpu
