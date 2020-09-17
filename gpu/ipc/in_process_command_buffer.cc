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
#include "cc/base/completion_event.h"
#include "components/viz/common/features.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/presentation_feedback_utils.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/swap_buffers_flags.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
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
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "gpu/ipc/shared_image_interface_in_process.h"
#include "gpu/ipc/single_task_sequence.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_bindings.h"
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

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace gpu {

namespace {

base::AtomicSequenceNumber g_next_route_id;
base::AtomicSequenceNumber g_next_image_id;

CommandBufferId NextCommandBufferId() {
  return CommandBufferIdFromChannelAndRoute(kDisplayCompositorClientId,
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

class ScopedEvent {
 public:
  explicit ScopedEvent(base::WaitableEvent* event) : event_(event) {}
  ~ScopedEvent() { event_->Signal(); }

 private:
  base::WaitableEvent* event_;
};

// Has to be called after Initialize.
void AddGLSurfaceRefOnGpuThread(gl::GLSurface* surface) {
  surface->AddRef();
}

void ReleaseGLSurfaceOnGpuThread(gl::GLSurface* surface,
                                 cc::CompletionEvent* event) {
  surface->Release();
  event->Signal();
}

void ReleaseGLSurfaceOnClientThread(gl::GLSurface* surface,
                                    CommandBufferTaskExecutor* task_executor) {
  cc::CompletionEvent event;
  task_executor->ScheduleOutOfOrderTask(base::BindOnce(
      &ReleaseGLSurfaceOnGpuThread, base::Unretained(surface), &event));
  event.Wait();
}

}  // namespace

InProcessCommandBuffer::SharedImageInterfaceHelper::SharedImageInterfaceHelper(
    InProcessCommandBuffer* command_buffer)
    : command_buffer_(command_buffer) {}

void InProcessCommandBuffer::SharedImageInterfaceHelper::SetError() {
  // Signal errors by losing the command buffer.
  command_buffer_->command_buffer_->SetParseError(error::kLostContext);
}

void InProcessCommandBuffer::SharedImageInterfaceHelper::WrapTaskWithGpuCheck(
    base::OnceClosure task) {
  command_buffer_->RunTaskOnGpuThread(std::move(task));
}

bool InProcessCommandBuffer::SharedImageInterfaceHelper::EnableWrappedSkImage()
    const {
  // We need WrappedSkImage to support creating a SharedImage with pixel data
  // when GL is unavailable. This is used in various unit tests.
  return command_buffer_->context_state_ &&
         !command_buffer_->context_state_->GrContextIsGL();
}

InProcessCommandBuffer::InProcessCommandBuffer(
    CommandBufferTaskExecutor* task_executor,
    const GURL& active_url)
    : command_buffer_id_(NextCommandBufferId()),
      active_url_(active_url),
      flush_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      task_executor_(task_executor),
      fence_sync_wait_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED) {
  // This binds the client sequence checker to the current sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  // Detach gpu sequence checker because we want to bind it to the gpu sequence,
  // and not the current (client) sequence except for webview (see Initialize).
  DETACH_FROM_SEQUENCE(gpu_sequence_checker_);
  DCHECK(task_executor_);
}

InProcessCommandBuffer::~InProcessCommandBuffer() {
  Destroy();
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

// This call happens after Initialize. Initialize blocks on finishing gpu thread
// task to create GLSurface, so when this call happens, we have a valid
// GLSurface.
base::ScopedClosureRunner InProcessCommandBuffer::GetCacheBackBufferCb() {
  // It is safe to use base::Unretained for |surface_| here since the we use a
  // synchronous task to create and destroy it from the client thread.
  task_executor_->ScheduleOutOfOrderTask(base::BindOnce(
      &AddGLSurfaceRefOnGpuThread, base::Unretained(surface_.get())));

  // Also safe to use base::Unretained for |task_executor_| since the caller is
  // supposed to guarentee that it outlives the callback.
  return base::ScopedClosureRunner(base::BindOnce(
      &ReleaseGLSurfaceOnClientThread, base::Unretained(surface_.get()),
      base::Unretained(task_executor_)));
}

bool InProcessCommandBuffer::MakeCurrent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!context_) {
    return true;
  }

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

base::Optional<gles2::ProgramCache::ScopedCacheUse>
InProcessCommandBuffer::CreateCacheUse() {
  base::Optional<gles2::ProgramCache::ScopedCacheUse> cache_use;
  if (context_group_->has_program_cache()) {
    cache_use.emplace(context_group_->get_program_cache(),
                      base::BindRepeating(&DecoderClient::CacheShader,
                                          base::Unretained(this)));
  }
  return cache_use;
}

gpu::ContextResult InProcessCommandBuffer::Initialize(
    scoped_refptr<gl::GLSurface> surface,
    bool is_offscreen,
    SurfaceHandle surface_handle,
    const ContextCreationAttribs& attribs,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    ImageFactory* image_factory,
    GpuChannelManagerDelegate* gpu_channel_manager_delegate,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    SingleTaskSequence* task_sequence,
    gpu::raster::GrShaderCache* gr_shader_cache,
    GpuProcessActivityFlags* activity_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::Initialize")

  is_offscreen_ = is_offscreen;
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

  client_thread_weak_ptr_ = client_thread_weak_ptr_factory_.GetWeakPtr();

  Capabilities capabilities;
  InitializeOnGpuThreadParams params(surface_handle, attribs, &capabilities,
                                     image_factory, gr_shader_cache,
                                     activity_flags);

  base::OnceCallback<gpu::ContextResult(void)> init_task =
      base::BindOnce(&InProcessCommandBuffer::InitializeOnGpuThread,
                     base::Unretained(this), params);

  // If a |task_sequence| is passed in, command buffer is meant to share it with
  // other users, take the raw pointer in this case because the |task_sequence|
  // would be kept alive by VizProcessContextProvider. If no |task_sequence| is
  // passed in, create one here.
  if (task_sequence) {
    task_sequence_ = task_sequence;
  } else {
    task_scheduler_holder_ =
        base::MakeRefCounted<gpu::GpuTaskSchedulerHelper>(task_executor_);
    task_sequence_ = task_scheduler_holder_->GetTaskSequence();
  }

  // Here we block by using a WaitableEvent to make sure InitializeOnGpuThread
  // is finished as part of Initialize function. This also makes sure we won't
  // try to cache GLSurface before the creation is finished.
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  task_sequence_->ScheduleTask(
      WrapTaskWithResult(std::move(init_task), &result, &completion), {});
  completion.Wait();

  if (result == gpu::ContextResult::kSuccess) {
    capabilities_ = capabilities;
    shared_image_interface_ = std::make_unique<SharedImageInterfaceInProcess>(
        task_executor_, task_sequence_, NextCommandBufferId(),
        context_group_->mailbox_manager(), image_factory_,
        context_group_->memory_tracker(),
        std::make_unique<SharedImageInterfaceHelper>(this));
  }

  return result;
}

gpu::ContextResult InProcessCommandBuffer::InitializeOnGpuThread(
    const InitializeOnGpuThreadParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::InitializeOnGpuThread")
  UpdateActiveUrl();

  if (gpu_channel_manager_delegate_ &&
      gpu_channel_manager_delegate_->IsExiting()) {
    LOG(ERROR) << "ContextResult::kTransientFailure: trying to create command "
                  "buffer during process shutdown.";
    return gpu::ContextResult::kTransientFailure;
  }

  GpuDriverBugWorkarounds workarounds(
      task_executor_->gpu_feature_info().enabled_gpu_driver_bug_workarounds);

  std::unique_ptr<MemoryTracker> memory_tracker;
  // Android WebView won't have a memory tracker.
  if (task_executor_->ShouldCreateMemoryTracker()) {
    const uint64_t client_tracing_id =
        base::trace_event::MemoryDumpManager::GetInstance()
            ->GetTracingProcessId();
    memory_tracker = std::make_unique<GpuCommandBufferMemoryTracker>(
        command_buffer_id_, client_tracing_id, params.attribs.context_type,
        base::ThreadTaskRunnerHandle::Get(), /* obserer=*/nullptr);
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
      task_executor_->gpu_feature_info(), task_executor_->discardable_manager(),
      task_executor_->passthrough_discardable_manager(),
      task_executor_->shared_image_manager());

#if defined(OS_MAC)
  // Virtualize GpuPreference:::kLowPower contexts by default on OS X to prevent
  // performance regressions when enabling FCM. https://crbug.com/180463
  use_virtualized_gl_context_ |=
      (params.attribs.gpu_preference == gl::GpuPreference::kLowPower);
#endif

  use_virtualized_gl_context_ |= task_executor_->ForceVirtualizedGLContexts();

  // MailboxManagerSync synchronization correctness currently depends on having
  // only a single context. See https://crbug.com/510243 for details.
  use_virtualized_gl_context_ |= task_executor_->mailbox_manager()->UsesSync();

  use_virtualized_gl_context_ |=
      context_group_->feature_info()->workarounds().use_virtualized_gl_contexts;

  if (context_group_->use_passthrough_cmd_decoder()) {
    // Virtualized contexts don't work with passthrough command decoder.
    // See https://crbug.com/914976
    use_virtualized_gl_context_ = false;
  }
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");

  command_buffer_ = std::make_unique<CommandBufferService>(
      this, context_group_->memory_tracker());

  context_state_ = task_executor_->GetSharedContextState();

  if (!surface_) {
    if (is_offscreen_) {
      if (context_state_) {
        surface_ = context_state_->surface();
      } else {
        // TODO(crbug.com/832243): GLES2CommandBufferStub has additional logic
        // for offscreen surfaces that might be needed here.
        surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
        if (!surface_.get()) {
          DestroyOnGpuThread();
          LOG(ERROR)
              << "ContextResult::kFatalFailure: Failed to create surface.";
          return gpu::ContextResult::kFatalFailure;
        }
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
#if defined(USE_OZONE)
      if (features::IsUsingOzonePlatform() &&
          params.surface_handle != gpu::kNullSurfaceHandle) {
        window_surface_ =
            ui::OzonePlatform::GetInstance()
                ->GetSurfaceFactoryOzone()
                ->CreatePlatformWindowSurface(params.surface_handle);
      }
#endif
      surface_ = ImageTransportSurface::CreateNativeSurface(
          gpu_thread_weak_ptr_factory_.GetWeakPtr(), params.surface_handle,
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

  sync_point_client_state_ =
      task_executor_->sync_point_manager()->CreateSyncPointClientState(
          GetNamespaceID(), GetCommandBufferID(),
          task_sequence_->GetSequenceId());

  if (context_group_->use_passthrough_cmd_decoder()) {
    // When using the passthrough command decoder, never share with other
    // contexts.
    gl_share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
  } else {
    // When using the validating command decoder, always use the global share
    // group.
    gl_share_group_ = task_executor_->GetShareGroup();
  }

  if (params.attribs.context_type == CONTEXT_TYPE_WEBGPU) {
    if (!task_executor_->gpu_preferences().enable_webgpu) {
      DLOG(ERROR) << "ContextResult::kFatalFailure: WebGPU not enabled";
      return gpu::ContextResult::kFatalFailure;
    }
    std::unique_ptr<webgpu::WebGPUDecoder> webgpu_decoder(
        webgpu::WebGPUDecoder::Create(
            this, command_buffer_.get(), task_executor_->shared_image_manager(),
            context_group_->memory_tracker(), task_executor_->outputter(),
            task_executor_->gpu_preferences()));
    gpu::ContextResult result = webgpu_decoder->Initialize();
    if (result != gpu::ContextResult::kSuccess) {
      DestroyOnGpuThread();
      DLOG(ERROR) << "Failed to initialize WebGPU decoder.";
      return result;
    }
    decoder_ = std::move(webgpu_decoder);
  } else {
    // TODO(khushalsagar): A lot of this initialization code is duplicated in
    // GpuChannelManager. Pull it into a common util method.
    scoped_refptr<gl::GLContext> real_context =
        use_virtualized_gl_context_ ? gl_share_group_->shared_context()
                                    : nullptr;
    if (real_context &&
        (!real_context->MakeCurrent(surface_.get()) ||
         real_context->CheckStickyGraphicsResetStatus() != GL_NO_ERROR)) {
      real_context = nullptr;
    }
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
        gl_share_group_->SetSharedContext(real_context.get());
    }

    if (!real_context->MakeCurrent(surface_.get())) {
      LOG(ERROR)
          << "ContextResult::kTransientFailure, failed to make context current";
      DestroyOnGpuThread();
      return ContextResult::kTransientFailure;
    }

    if (params.attribs.enable_raster_interface &&
        !params.attribs.enable_gles2_interface) {
      gr_shader_cache_ = params.gr_shader_cache;

      if (!context_state_ ||
          !context_state_->MakeCurrent(nullptr, /*needs_gl=*/true)) {
        DestroyOnGpuThread();
        LOG(ERROR) << "Failed to make context current.";
        return ContextResult::kTransientFailure;
      }

      // TODO(penghuang): Merge all SharedContextState::Initialize*()
      if (!context_state_->IsGLInitialized()) {
        context_state_->InitializeGL(task_executor_->gpu_preferences(),
                                     context_group_->feature_info());
      }

      if (base::ThreadTaskRunnerHandle::IsSet()) {
        gr_cache_controller_.emplace(context_state_.get(),
                                     base::ThreadTaskRunnerHandle::Get());
      }

      context_ = context_state_->context();
      decoder_.reset(raster::RasterDecoder::Create(
          this, command_buffer_.get(), task_executor_->outputter(),
          task_executor_->gpu_feature_info(), task_executor_->gpu_preferences(),
          context_group_->memory_tracker(),
          task_executor_->shared_image_manager(), context_state_,
          true /*is_privileged*/));
    } else {
      decoder_.reset(gles2::GLES2Decoder::Create(this, command_buffer_.get(),
                                                 task_executor_->outputter(),
                                                 context_group_.get()));
      if (use_virtualized_gl_context_) {
        context_ = base::MakeRefCounted<GLContextVirtual>(
            gl_share_group_.get(), real_context.get(), decoder_->AsWeakPtr());
        if (!context_->Initialize(surface_.get(),
                                  GenerateGLContextAttribs(
                                      params.attribs, context_group_.get()))) {
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
      } else {
        context_ = real_context;
        DCHECK(context_->IsCurrent(surface_.get()));
      }
    }

    if (!context_group_->has_program_cache() &&
        !context_group_->feature_info()->workarounds().disable_program_cache) {
      context_group_->set_program_cache(task_executor_->program_cache());
    }
  }

  gles2::DisallowedFeatures disallowed_features;
  auto result = decoder_->Initialize(surface_, context_, is_offscreen_,
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

  if (gpu_channel_manager_delegate_) {
    gpu_channel_manager_delegate_->DidCreateContextSuccessfully();
    gpu_channel_manager_delegate_->RegisterDisplayContext(this);
  }

  return gpu::ContextResult::kSuccess;
}

void InProcessCommandBuffer::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::Destroy");

  client_thread_weak_ptr_factory_.InvalidateWeakPtrs();
  gpu_control_client_ = nullptr;
  shared_image_interface_ = nullptr;
  // Here we block by using a WaitableEvent to make sure DestroyOnGpuThread is
  // finished as part of Destroy.
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool result = false;
  base::OnceCallback<bool(void)> destroy_task = base::BindOnce(
      &InProcessCommandBuffer::DestroyOnGpuThread, base::Unretained(this));
  task_sequence_->ScheduleTask(
      WrapTaskWithResult(std::move(destroy_task), &result, &completion), {});

  completion.Wait();
  task_sequence_ = nullptr;
}

bool InProcessCommandBuffer::DestroyOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::DestroyOnGpuThread");
  UpdateActiveUrl();

  if (gpu_channel_manager_delegate_)
    gpu_channel_manager_delegate_->UnregisterDisplayContext(this);

  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");
  gpu_thread_weak_ptr_factory_.InvalidateWeakPtrs();
  // Clean up GL resources if possible.
  bool have_context = context_.get() && context_->MakeCurrent(surface_.get());
  base::Optional<gles2::ProgramCache::ScopedCacheUse> cache_use;
  if (have_context)
    cache_use = CreateCacheUse();

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
  surface_ = nullptr;
#if defined(USE_OZONE)
  window_surface_.reset();
#endif

  context_ = nullptr;
  if (sync_point_client_state_) {
    sync_point_client_state_->Destroy();
    sync_point_client_state_ = nullptr;
  }
  gl_share_group_ = nullptr;
  context_group_ = nullptr;
  if (context_state_)
    context_state_->MakeCurrent(nullptr);
  context_state_ = nullptr;
  return true;
}

CommandBufferServiceClient::CommandBatchProcessedResult
InProcessCommandBuffer::OnCommandBatchProcessed() {
  return task_sequence_->ShouldYield() ? kPauseExecution : kContinueExecution;
}

void InProcessCommandBuffer::OnParseError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  // There is a race between service side FlushOnGpuThread() calling
  // UpdateLastStateOnGpuThread() and client side calling GetLastState().
  // Update last_state_ now before notifying client side to save the
  // error and make the race benign.
  UpdateLastStateOnGpuThread();

  CommandBuffer::State state = command_buffer_->GetState();

  if (gpu_channel_manager_delegate_) {
    // Tell the browser about this context loss so it can determine whether
    // client APIs like WebGL need to be blocked from automatically running.
    // |offscreen| is used to determine if it's compositing context or not.
    gpu_channel_manager_delegate_->DidLoseContext(
        /*offscreen=*/false, state.context_lost_reason, active_url_.url());

    // Check the error reason and robustness extension to get a better idea if
    // the GL context was lost. We might try restarting the GPU process to
    // recover from actual GL context loss but it's unnecessary for other types
    // of parse errors.
    if (state.error == error::kLostContext) {
      bool was_lost_by_robustness =
          decoder_ && decoder_->WasContextLostByRobustnessExtension();

      if (was_lost_by_robustness) {
        GpuDriverBugWorkarounds workarounds(
            GetGpuFeatureInfo().enabled_gpu_driver_bug_workarounds);

        // Lose all other contexts.
        if (gl::GLContext::LosesAllContextsOnContextLost() ||
            (context_state_ && context_state_->use_virtualized_gl_contexts())) {
          gpu_channel_manager_delegate_->LoseAllContexts();
        }

        // Work around issues with recovery by allowing a new GPU process to
        // launch.
        if (workarounds.exit_on_context_lost)
          gpu_channel_manager_delegate_->MaybeExitOnContextLost();
      }
    }
  }

  PostOrRunClientCallback(base::BindOnce(&InProcessCommandBuffer::OnContextLost,
                                         client_thread_weak_ptr_));
}

void InProcessCommandBuffer::MarkContextLost() {
  if (!command_buffer_ ||
      command_buffer_->GetState().error == error::kLostContext) {
    return;
  }

  command_buffer_->SetContextLostReason(error::kUnknown);
  if (decoder_)
    decoder_->MarkContextLost(error::kUnknown);
  command_buffer_->SetParseError(error::kLostContext);
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
  UpdateActiveUrl();
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

void InProcessCommandBuffer::FlushOnGpuThread(
    int32_t put_offset,
    const std::vector<SyncToken>& sync_token_fences,
    base::TimeTicks flush_timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  TRACE_EVENT1("gpu", "InProcessCommandBuffer::FlushOnGpuThread", "put_offset",
               put_offset);

  if (!flush_timestamp.is_null()) {
    viz_scheduled_draw_ = flush_timestamp;
    gpu_started_draw_ = base::TimeTicks::Now();
  }

  ScopedEvent handle_flush(&flush_event_);
  // Check if sync token waits are invalid or already complete. Do not use
  // SyncPointManager::IsSyncTokenReleased() as it can't say if the wait is
  // invalid.
  for (const auto& sync_token : sync_token_fences)
    DCHECK(!sync_point_client_state_->Wait(sync_token, base::DoNothing()));

  if (!MakeCurrent())
    return;
  auto cache_use = CreateCacheUse();

  MailboxManager* mailbox_manager = context_group_->mailbox_manager();
  if (mailbox_manager->UsesSync()) {
    for (const auto& sync_token : sync_token_fences)
      mailbox_manager->PullTextureUpdates(sync_token);
  }

  {
    base::Optional<raster::GrShaderCache::ScopedCacheUse> gr_cache_use;
    if (gr_shader_cache_)
      gr_cache_use.emplace(gr_shader_cache_, kDisplayCompositorClientId);
    command_buffer_->Flush(put_offset, decoder_.get());
  }
  // Update state before signaling the flush event.
  UpdateLastStateOnGpuThread();

  bool has_unprocessed_commands = HasUnprocessedCommandsOnGpuThread();

  if (!command_buffer_->scheduled() || has_unprocessed_commands) {
    ContinueGpuTask(base::BindOnce(&InProcessCommandBuffer::FlushOnGpuThread,
                                   gpu_thread_weak_ptr_factory_.GetWeakPtr(),
                                   put_offset, sync_token_fences,
                                   base::TimeTicks()));
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
    auto cache_use = CreateCacheUse();
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
      base::BindOnce(&InProcessCommandBuffer::PerformDelayedWorkOnGpuThread,
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

  std::vector<SyncToken> sync_token_fences;
  next_flush_sync_token_fences_.swap(sync_token_fences);

  base::TimeTicks flush_timestamp;
  if (should_measure_next_flush_) {
    should_measure_next_flush_ = false;
    flush_timestamp = base::TimeTicks::Now();
  }

  // Don't use std::move() for |sync_token_fences| because evaluation order for
  // arguments is not defined.
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::FlushOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), put_offset,
                     sync_token_fences, flush_timestamp),
      sync_token_fences);
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
    uint32_t size,
    int32_t* id,
    TransferBufferAllocationOption option) {
  scoped_refptr<Buffer> buffer = MakeMemoryBuffer(size);
  *id = GetNextBufferId();
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
  if (requires_sync_point)
    fence_sync = GenerateFenceSyncRelease();

  ScheduleGpuTask(base::BindOnce(
      &InProcessCommandBuffer::CreateImageOnGpuThread,
      gpu_thread_weak_ptr_factory_.GetWeakPtr(), new_id, std::move(handle),
      gfx::Size(base::checked_cast<int>(width),
                base::checked_cast<int>(height)),
      gpu_memory_buffer->GetFormat(), fence_sync));

  if (fence_sync) {
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

  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      if (!base::IsValueInRangeForNumericType<size_t>(handle.stride)) {
        LOG(ERROR) << "Invalid stride for image.";
        return;
      }
      auto image = base::MakeRefCounted<gl::GLImageSharedMemory>(size);
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
              std::move(handle), size, format, kDisplayCompositorClientId,
              kNullSurfaceHandle);
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
    gpu_channel_manager_delegate_->StoreShaderToDisk(kDisplayCompositorClientId,
                                                     key, shader);
}

void InProcessCommandBuffer::OnFenceSyncRelease(uint64_t release) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  SyncToken sync_token(GetNamespaceID(), GetCommandBufferID(), release);

  MailboxManager* mailbox_manager = context_group_->mailbox_manager();
  if (mailbox_manager->UsesSync())
    mailbox_manager->PushTextureUpdates(sync_token);

  command_buffer_->SetReleaseCount(release);
  sync_point_client_state_->ReleaseFenceSync(release);
}

void InProcessCommandBuffer::OnDescheduleUntilFinished() {
  NOTREACHED();
}

void InProcessCommandBuffer::OnRescheduleAfterFinished() {
  NOTREACHED();
}

void InProcessCommandBuffer::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  pending_swap_completed_params_.push_back(
      {swap_id, flags, viz_scheduled_draw_, gpu_started_draw_});
  pending_presented_params_.push_back({swap_id, flags});
  viz_scheduled_draw_ = base::TimeTicks();
  gpu_started_draw_ = base::TimeTicks();
}

void InProcessCommandBuffer::ScheduleGrContextCleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (gr_cache_controller_)
    gr_cache_controller_->ScheduleGrContextCleanup();
}

void InProcessCommandBuffer::HandleReturnData(base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  std::vector<uint8_t> vec(data.data(), data.data() + data.size());
  PostOrRunClientCallback(
      base::BindOnce(&InProcessCommandBuffer::HandleReturnDataOnOriginThread,
                     client_thread_weak_ptr_, std::move(vec)));
}

void InProcessCommandBuffer::PostOrRunClientCallback(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!origin_task_runner_) {
    task_executor_->PostNonNestableToClient(std::move(callback));
    return;
  }
  origin_task_runner_->PostTask(FROM_HERE, std::move(callback));
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
  gfx::GpuFenceHandle handle = gpu_fence->GetGpuFenceHandle().Clone();

  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::CreateGpuFenceOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), gpu_fence_id,
                     std::move(handle)));
}

void InProcessCommandBuffer::CreateGpuFenceOnGpuThread(
    uint32_t gpu_fence_id,
    gfx::GpuFenceHandle handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  UpdateActiveUrl();

  if (!GetFeatureInfo()->feature_flags().chromium_gpu_fence) {
    DLOG(ERROR) << "CHROMIUM_gpu_fence unavailable";
    command_buffer_->SetParseError(error::kLostContext);
    return;
  }

  gles2::GpuFenceManager* gpu_fence_manager = decoder_->GetGpuFenceManager();
  DCHECK(gpu_fence_manager);

  if (gpu_fence_manager->CreateGpuFenceFromHandle(gpu_fence_id,
                                                  std::move(handle)))
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

void InProcessCommandBuffer::WaitSyncToken(const SyncToken& sync_token) {
  next_flush_sync_token_fences_.push_back(sync_token);
}

bool InProcessCommandBuffer::CanWaitUnverifiedSyncToken(
    const SyncToken& sync_token) {
  return sync_token.namespace_id() == GetNamespaceID();
}

void InProcessCommandBuffer::SetDisplayTransform(
    gfx::OverlayTransform transform) {
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::SetDisplayTransformOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), transform));
}

void InProcessCommandBuffer::SetDisplayTransformOnGpuThread(
    gfx::OverlayTransform transform) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  surface_->SetDisplayTransform(transform);
}

void InProcessCommandBuffer::SetFrameRate(float frame_rate) {
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::SetFrameRateOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), frame_rate));
}

void InProcessCommandBuffer::SetFrameRateOnGpuThread(float frame_rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  surface_->SetFrameRate(frame_rate);
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

  auto& pending_swap = pending_swap_completed_params_.front();

  params.swap_response.timings.viz_scheduled_draw =
      pending_swap.viz_scheduled_draw;
  params.swap_response.timings.gpu_started_draw = pending_swap.gpu_started_draw;
  params.swap_response.swap_id = pending_swap.swap_id;
  pending_swap_completed_params_.pop_front();

  PostOrRunClientCallback(base::BindOnce(
      &InProcessCommandBuffer::DidSwapBuffersCompleteOnOriginThread,
      client_thread_weak_ptr_, std::move(params)));
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

  PostOrRunClientCallback(base::BindOnce(
      &InProcessCommandBuffer::BufferPresentedOnOriginThread,
      client_thread_weak_ptr_, params.swap_id, params.flags, feedback));
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

  if (update_vsync_parameters_callback_ && ShouldUpdateVsyncParams(feedback)) {
    update_vsync_parameters_callback_.Run(feedback.timestamp,
                                          feedback.interval);
  }
}

void InProcessCommandBuffer::HandleReturnDataOnOriginThread(
    std::vector<uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  if (gpu_control_client_) {
    gpu_control_client_->OnGpuControlReturnData(data);
  }
}

void InProcessCommandBuffer::SetUpdateVSyncParametersCallback(
    viz::UpdateVSyncParametersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  update_vsync_parameters_callback_ = std::move(callback);
}

void InProcessCommandBuffer::UpdateActiveUrl() {
  if (!active_url_.is_empty())
    ContextUrl::SetActiveUrl(active_url_);
}

void InProcessCommandBuffer::SetGpuVSyncCallback(
    viz::GpuVSyncCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  gpu_vsync_callback_ = std::move(callback);
}

void InProcessCommandBuffer::SetGpuVSyncEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::SetGpuVSyncEnabledOnThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), enabled));
}

void InProcessCommandBuffer::SetGpuVSyncEnabledOnThread(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (surface_)
    surface_->SetGpuVSyncEnabled(enabled);
}

viz::GpuVSyncCallback InProcessCommandBuffer::GetGpuVSyncCallback() {
  auto handle_gpu_vsync_callback =
      base::BindRepeating(&InProcessCommandBuffer::HandleGpuVSyncOnOriginThread,
                          client_thread_weak_ptr_);
  auto forward_callback =
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         viz::GpuVSyncCallback callback, base::TimeTicks vsync_time,
         base::TimeDelta vsync_interval) {
        task_runner->PostTask(
            FROM_HERE, base::BindOnce(callback, vsync_time, vsync_interval));
      };
  return base::BindRepeating(forward_callback,
                             base::RetainedRef(origin_task_runner_),
                             std::move(handle_gpu_vsync_callback));
}

base::TimeDelta InProcessCommandBuffer::GetGpuBlockedTimeSinceLastSwap() {
  // Some examples and tests create InProcessCommandBuffer without
  // GpuChannelManagerDelegate.
  if (!gpu_channel_manager_delegate_)
    return base::TimeDelta::Min();

  return gpu_channel_manager_delegate_->GetGpuScheduler()
      ->TakeTotalBlockingTime();
}

void InProcessCommandBuffer::HandleGpuVSyncOnOriginThread(
    base::TimeTicks vsync_time,
    base::TimeDelta vsync_interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  if (gpu_vsync_callback_)
    gpu_vsync_callback_.Run(vsync_time, vsync_interval);
}

void InProcessCommandBuffer::SetNeedsMeasureNextDrawLatency() {
  should_measure_next_flush_ = true;
}

}  // namespace gpu
