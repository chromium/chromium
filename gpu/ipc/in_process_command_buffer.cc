// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/in_process_command_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/completion_event.h"
#include "components/viz/common/features.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_fence_manager.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/memory_program_cache.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/raster_decoder.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_interface_in_process.h"
#include "gpu/command_buffer/service/single_task_sequence.h"
#include "gpu/command_buffer/service/task_graph.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/create_gr_gl_interface.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

namespace {

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
  raw_ptr<base::WaitableEvent> event_;
};

}  // namespace

InProcessCommandBuffer::InitializeOnGpuThreadParams::
    InitializeOnGpuThreadParams(mojom::ContextCreationAttribsPtr attribs,
                                Capabilities* capabilities,
                                GLCapabilities* gl_capabilities,
                                gpu::raster::GrShaderCache* gr_shader_cache,
                                GpuProcessShmCount* use_shader_cache_shm_count)
    : attribs(std::move(attribs)),
      capabilities(capabilities),
      gl_capabilities(gl_capabilities),
      gr_shader_cache(gr_shader_cache),
      use_shader_cache_shm_count(use_shader_cache_shm_count) {}

InProcessCommandBuffer::InitializeOnGpuThreadParams::
    ~InitializeOnGpuThreadParams() = default;

InProcessCommandBuffer::InitializeOnGpuThreadParams::
    InitializeOnGpuThreadParams(InitializeOnGpuThreadParams&& other) = default;
InProcessCommandBuffer::InitializeOnGpuThreadParams&
InProcessCommandBuffer::InitializeOnGpuThreadParams::operator=(
    InitializeOnGpuThreadParams&& other) = default;

InProcessCommandBuffer::InProcessCommandBuffer(
    CommandBufferTaskExecutor* task_executor,
    const GURL& active_url)
    : command_buffer_id_(
          DisplayCompositorMemoryAndTaskControllerOnGpu::NextCommandBufferId()),
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

webgpu::WebGPUDecoder* InProcessCommandBuffer::GetWebGPUDecoderForTest() const {
  return static_cast<webgpu::WebGPUDecoder*>(decoder_.get());
}

gpu::SharedImageInterface* InProcessCommandBuffer::GetSharedImageInterface()
    const {
  return shared_image_interface_.get();
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

void InProcessCommandBuffer::CreateCacheUse(
    std::optional<gles2::ProgramCache::ScopedCacheUse>& cache_use) {
  if (context_group_->has_program_cache()) {
    cache_use.emplace(
        context_group_->get_program_cache(),
        base::BindRepeating(&DecoderClient::CacheBlob, base::Unretained(this),
                            gpu::GpuDiskCacheType::kGlShaders));
  }
}

gpu::ContextResult InProcessCommandBuffer::Initialize(
    mojom::ContextCreationAttribsPtr attribs,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gpu::raster::GrShaderCache* gr_shader_cache,
    GpuProcessShmCount* use_shader_cache_shm_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::Initialize");

  DCHECK(task_runner);
  origin_task_runner_ = std::move(task_runner);

  client_thread_weak_ptr_ = client_thread_weak_ptr_factory_.GetWeakPtr();

  Capabilities capabilities;
  GLCapabilities gl_capabilities;
  InitializeOnGpuThreadParams params(std::move(attribs), &capabilities,
                                     &gl_capabilities, gr_shader_cache,
                                     use_shader_cache_shm_count);

  base::OnceCallback<gpu::ContextResult(void)> init_task =
      base::BindOnce(&InProcessCommandBuffer::InitializeOnGpuThread,
                     base::Unretained(this), std::move(params));

  task_scheduler_holder_ =
      std::make_unique<gpu::GpuTaskSchedulerHelper>(task_executor_);
  task_sequence_ = task_scheduler_holder_->GetTaskSequence();

  // Here we block by using a WaitableEvent to make sure InitializeOnGpuThread
  // is finished as part of Initialize function. This also makes sure we won't
  // try to cache GLSurface before the creation is finished.
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  task_sequence_->ScheduleTask(
      WrapTaskWithResult(std::move(init_task), &result, &completion),
      /*sync_token_fences=*/{}, SyncToken());
  completion.Wait();

  if (result == gpu::ContextResult::kSuccess) {
    capabilities_ = capabilities;
    gl_capabilities_ = gl_capabilities;
    shared_image_interface_ = SharedImageInterfaceInProcess::Create(
        task_sequence_, task_executor_->gpu_preferences(),
        context_group_->feature_info()->workarounds(),
        task_executor_->gpu_feature_info(), context_state_.get(),
        task_executor_->shared_image_manager(),
        /*is_for_display_compositor=*/false, task_executor_->GetTaskRunner());
  }

  return result;
}

gpu::ContextResult InProcessCommandBuffer::InitializeOnGpuThread(
    const InitializeOnGpuThreadParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::InitializeOnGpuThread");
  UpdateActiveUrl();

  GpuDriverBugWorkarounds workarounds(
      task_executor_->gpu_feature_info().enabled_gpu_driver_bug_workarounds);

  scoped_refptr<MemoryTracker> memory_tracker;
  // Android WebView won't have a memory tracker.
  if (task_executor_->ShouldCreateMemoryTracker()) {
    const uint64_t client_tracing_id =
        base::trace_event::MemoryDumpManager::GetInstance()
            ->GetTracingProcessId();
    memory_tracker = base::MakeRefCounted<MemoryTracker>(
        GetCommandBufferID(), client_tracing_id,
        /*peak_memory_monitor=*/nullptr,
        GpuPeakMemoryAllocationSource::COMMAND_BUFFER);
  }

  auto feature_info = base::MakeRefCounted<gles2::FeatureInfo>(
      workarounds, task_executor_->gpu_feature_info());
  context_group_ = base::MakeRefCounted<gles2::ContextGroup>(
      task_executor_->gpu_preferences(), std::move(memory_tracker),
      task_executor_->shader_translator_cache(),
      task_executor_->framebuffer_completeness_cache(), feature_info,
      /*progress_reporter=*/nullptr, task_executor_->gpu_feature_info(),
      task_executor_->shared_image_manager());

#if BUILDFLAG(IS_MAC)
  // Virtualize GpuPreference:::kLowPower contexts by default on OS X to prevent
  // performance regressions when enabling FCM. https://crbug.com/180463
  use_virtualized_gl_context_ |= params.attribs->is_gles() &&
                                 (params.attribs->get_gles()->gpu_preference ==
                                  gl::GpuPreference::kLowPower);
#endif

  use_virtualized_gl_context_ |= task_executor_->ForceVirtualizedGLContexts();

  use_virtualized_gl_context_ |=
      context_group_->feature_info()->workarounds().use_virtualized_gl_contexts;

  if (context_group_->use_passthrough_cmd_decoder()) {
    // Virtualized contexts don't work with passthrough command decoder.
    // See https://crbug.com/914976
    use_virtualized_gl_context_ = false;
  }

  command_buffer_ = std::make_unique<CommandBufferService>(
      this, context_group_->memory_tracker());

  context_state_ = task_executor_->GetSharedContextState();

  scoped_refptr<gl::GLSurface> surface;
  if (context_state_) {
    surface = context_state_->surface();
  } else {
    // TODO(crbug.com/40196979): Is creating an offscreen GL surface needed
    // still?
    surface = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(),
                                                 gfx::Size());
    if (!surface) {
      DestroyOnGpuThread();
      LOG(ERROR) << "ContextResult::kFatalFailure: Failed to create surface.";
      return gpu::ContextResult::kFatalFailure;
    }
  }

  sync_point_client_state_ = task_sequence_->CreateSyncPointClientState(
      GetNamespaceID(), GetCommandBufferID());

  if (context_group_->use_passthrough_cmd_decoder()) {
    // When using the passthrough command decoder, never share with other
    // contexts.
    gl_share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
  } else {
    // When using the validating command decoder, always use the global share
    // group.
    gl_share_group_ = task_executor_->GetShareGroup();
  }

  switch (params.attribs->which()) {
    case mojom::ContextCreationAttribs::Tag::kWebgpu: {
      if (!task_executor_->gpu_preferences().enable_webgpu) {
        DLOG(ERROR) << "ContextResult::kFatalFailure: WebGPU not enabled";
        return gpu::ContextResult::kFatalFailure;
      }
      std::unique_ptr<webgpu::WebGPUDecoder> webgpu_decoder =
          webgpu::WebGPUDecoder::Create(
              this, command_buffer_.get(),
              task_executor_->shared_image_manager(),
              context_group_->memory_tracker(), task_executor_->outputter(),
              task_executor_->gpu_preferences(), context_state_);
      gpu::ContextResult result =
          webgpu_decoder->Initialize(task_executor_->gpu_feature_info());
      if (result != gpu::ContextResult::kSuccess) {
        DestroyOnGpuThread();
        DLOG(ERROR) << "Failed to initialize WebGPU decoder.";
        return result;
      }

      decoder_ = std::move(webgpu_decoder);
    } break;
    case mojom::ContextCreationAttribs::Tag::kRaster: {
      // RasterDecoder uses the shared context.
      use_virtualized_gl_context_ = false;

      gr_shader_cache_ = params.gr_shader_cache.get();

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

      context_ = context_state_->context();
      std::unique_ptr<raster::RasterDecoder> raster_decoder =
          raster::RasterDecoder::Create(
              this, command_buffer_.get(), task_executor_->outputter(),
              task_executor_->gpu_feature_info(),
              task_executor_->gpu_preferences(),
              context_group_->memory_tracker(),
              task_executor_->shared_image_manager(), context_state_,
              /*is_privileged=*/true);

      const auto& attribs = params.attribs->get_raster();
      auto result =
          raster_decoder->Initialize(attribs->lose_context_when_out_of_memory);
      if (result != gpu::ContextResult::kSuccess) {
        DestroyOnGpuThread();
        DLOG(ERROR) << "Failed to initialize decoder.";
        return result;
      }

      decoder_ = std::move(raster_decoder);
    } break;
    case mojom::ContextCreationAttribs::Tag::kGles: {
      const auto& attribs = params.attribs->get_gles();
      // TODO(khushalsagar): A lot of this initialization code is duplicated in
      // GpuChannelManager. Pull it into a common util method.
      scoped_refptr<gl::GLContext> real_context =
          use_virtualized_gl_context_ ? gl_share_group_->shared_context()
                                      : nullptr;
      if (real_context &&
          (!real_context->MakeCurrent(surface.get()) ||
           real_context->CheckStickyGraphicsResetStatus() != GL_NO_ERROR)) {
        real_context = nullptr;
      }
      if (!real_context) {
        real_context = gl::init::CreateGLContext(
            gl_share_group_.get(), surface.get(),
            GenerateGLContextAttribsForDecoder(attribs->context_type,
                                               attribs->gpu_preference,
                                               context_group_.get()));
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

      if (!real_context->MakeCurrent(surface.get())) {
        LOG(ERROR) << "ContextResult::kTransientFailure, failed to make "
                      "context current";
        DestroyOnGpuThread();
        return ContextResult::kTransientFailure;
      }

      std::unique_ptr<gles2::GLES2Decoder> gles2_decoder =
          gles2::GLES2Decoder::Create(this, command_buffer_.get(),
                                      task_executor_->outputter(),
                                      context_group_.get());
      if (use_virtualized_gl_context_) {
        context_ = base::MakeRefCounted<GLContextVirtual>(
            gl_share_group_.get(), real_context.get(),
            gles2_decoder->AsWeakPtr());
        if (!context_->Initialize(
                surface.get(),
                GenerateGLContextAttribsForDecoder(attribs->context_type,
                                                   attribs->gpu_preference,
                                                   context_group_.get()))) {
          // TODO(piman): This might not be fatal, we could recurse into
          // CreateGLContext to get more info, tho it should be exceedingly
          // rare and may not be recoverable anyway.
          DestroyOnGpuThread();
          LOG(ERROR) << "ContextResult::kFatalFailure: "
                        "Failed to initialize virtual GL context.";
          return gpu::ContextResult::kFatalFailure;
        }

        if (!context_->MakeCurrent(surface.get())) {
          DestroyOnGpuThread();
          // The caller should retry making a context, but this one won't work.
          LOG(ERROR) << "ContextResult::kTransientFailure: "
                        "Could not make context current.";
          return gpu::ContextResult::kTransientFailure;
        }
      } else {
        context_ = real_context;
        DCHECK(context_->IsCurrent(surface.get()));
      }
      auto result = gles2_decoder->Initialize(
          surface, context_, /*offscreen=*/true, attribs->context_type,
          attribs->lose_context_when_out_of_memory);
      if (result != gpu::ContextResult::kSuccess) {
        DestroyOnGpuThread();
        DLOG(ERROR) << "Failed to initialize decoder.";
        return result;
      }

      decoder_ = std::move(gles2_decoder);
      if (!context_group_->has_program_cache() &&
          !context_group_->feature_info()
               ->workarounds()
               .disable_program_cache) {
        context_group_->set_program_cache(task_executor_->program_cache());
      }
      DCHECK(context_->default_surface());
    } break;
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
    if (!context_->MakeCurrent(surface.get())) {
      DestroyOnGpuThread();
      LOG(ERROR) << "ContextResult::kTransientFailure: "
                    "Failed to make context current after initialization.";
      return gpu::ContextResult::kTransientFailure;
    }
  }

  *params.capabilities = decoder_->GetCapabilities();
  *params.gl_capabilities = decoder_->GetGLCapabilities();

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
      WrapTaskWithResult(std::move(destroy_task), &result, &completion),
      /*sync_token_fences=*/{}, SyncToken());

  completion.Wait();
  task_sequence_ = nullptr;
}

bool InProcessCommandBuffer::DestroyOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  TRACE_EVENT0("gpu", "InProcessCommandBuffer::DestroyOnGpuThread");
  UpdateActiveUrl();

  gpu_thread_weak_ptr_factory_.InvalidateWeakPtrs();
  // Clean up GL resources if possible.
  bool have_context = context_.get() && context_->MakeCurrentDefault();
  std::optional<gles2::ProgramCache::ScopedCacheUse> cache_use;
  if (have_context)
    CreateCacheUse(cache_use);

  if (decoder_) {
    decoder_->Destroy(have_context);
    decoder_.reset();
  }
  command_buffer_.reset();

  context_ = nullptr;
  sync_point_client_state_.Reset();
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

  PostOrRunClientCallback(base::BindOnce(&InProcessCommandBuffer::OnContextLost,
                                         client_thread_weak_ptr_));
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

void InProcessCommandBuffer::RunTaskCallbackOnGpuThread(
    TaskCallback task,
    FenceSyncReleaseDelegate* release_delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  UpdateActiveUrl();
  std::move(task).Run(release_delegate);
}

void InProcessCommandBuffer::RunTaskClosureOnGpuThread(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  UpdateActiveUrl();
  std::move(task).Run();
}

void InProcessCommandBuffer::ScheduleGpuTask(
    TaskCallback task,
    std::vector<SyncToken> sync_token_fences,
    const SyncToken& release) {
  TaskCallback gpu_task = base::BindOnce(
      &InProcessCommandBuffer::RunTaskCallbackOnGpuThread,
      gpu_thread_weak_ptr_factory_.GetWeakPtr(), std::move(task));
  task_sequence_->ScheduleTask(std::move(gpu_task),
                               std::move(sync_token_fences), release);
}
void InProcessCommandBuffer::ScheduleGpuTask(
    base::OnceClosure task,
    std::vector<SyncToken> sync_token_fences,
    const SyncToken& release) {
  base::OnceClosure gpu_task = base::BindOnce(
      &InProcessCommandBuffer::RunTaskClosureOnGpuThread,
      gpu_thread_weak_ptr_factory_.GetWeakPtr(), std::move(task));
  task_sequence_->ScheduleTask(std::move(gpu_task),
                               std::move(sync_token_fences), release);
}

void InProcessCommandBuffer::ContinueGpuTask(TaskCallback task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  TaskCallback gpu_task = base::BindOnce(
      &InProcessCommandBuffer::RunTaskCallbackOnGpuThread,
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
    FenceSyncReleaseDelegate* release_delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  TRACE_EVENT1("gpu", "InProcessCommandBuffer::FlushOnGpuThread", "put_offset",
               put_offset);

  // Reentrant call is not supported.
  CHECK(!release_delegate_);
  base::AutoReset<raw_ptr<FenceSyncReleaseDelegate>> auto_reset(
      &release_delegate_, release_delegate);

  ScopedEvent handle_flush(&flush_event_);

  if (!MakeCurrent())
    return;
  std::optional<gles2::ProgramCache::ScopedCacheUse> cache_use;
  CreateCacheUse(cache_use);

  {
    std::optional<raster::GrShaderCache::ScopedCacheUse> gr_cache_use;
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

  if (MakeCurrent()) {
    std::optional<gles2::ProgramCache::ScopedCacheUse> cache_use;
    CreateCacheUse(cache_use);
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

  SyncToken release(GetNamespaceID(), GetCommandBufferID(),
                    next_fence_sync_release_ - 1);
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::FlushOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), put_offset),
      std::move(sync_token_fences), release);
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
    uint32_t alignment,
    TransferBufferAllocationOption option) {
  constexpr uint32_t kDefaultAlignment = 16;
  uint32_t buffer_alignment = std::max(alignment, kDefaultAlignment);
  scoped_refptr<Buffer> buffer = MakeMemoryBuffer(size, buffer_alignment);
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

void InProcessCommandBuffer::ForceLostContext(error::ContextLostReason reason) {
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::ForceLostContextOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), reason));
}

void InProcessCommandBuffer::ForceLostContextOnGpuThread(
    error::ContextLostReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  // Similar implementation to CommandBufferDirect.
  command_buffer_->SetContextLostReason(reason);
  command_buffer_->SetParseError(error::kLostContext);
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

const GLCapabilities& InProcessCommandBuffer::GetGLCapabilities() const {
  return gl_capabilities_;
}

const GpuFeatureInfo& InProcessCommandBuffer::GetGpuFeatureInfo() const {
  return task_executor_->gpu_feature_info();
}

void InProcessCommandBuffer::OnConsoleMessage(int32_t id,
                                              const std::string& message) {
  // TODO(piman): implement this.
}

void InProcessCommandBuffer::CacheBlob(gpu::GpuDiskCacheType type,
                                       const std::string& key,
                                       const std::string& shader) {}

void InProcessCommandBuffer::OnFenceSyncRelease(uint64_t release) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  command_buffer_->SetReleaseCount(release);

  CHECK(release_delegate_);
  release_delegate_->Release(release);
}

void InProcessCommandBuffer::OnDescheduleUntilFinished() {
  NOTREACHED();
}

void InProcessCommandBuffer::OnRescheduleAfterFinished() {
  NOTREACHED();
}

void InProcessCommandBuffer::ScheduleGrContextCleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  context_state_->ScheduleSkiaCleanup();
}

void InProcessCommandBuffer::HandleReturnData(base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  std::vector<uint8_t> vec(data.data(), UNSAFE_TODO(data.data() + data.size()));
  PostOrRunClientCallback(
      base::BindOnce(&InProcessCommandBuffer::HandleReturnDataOnOriginThread,
                     client_thread_weak_ptr_, std::move(vec)));
}

bool InProcessCommandBuffer::ShouldYield() {
  return task_sequence_->ShouldYield();
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
  ScheduleGpuTask(WrapClientCallback(std::move(callback)),
                  /*sync_token_fences=*/{sync_token});
}

void InProcessCommandBuffer::SignalQuery(unsigned query_id,
                                         base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::SignalQueryOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr(), query_id,
                     std::move(callback)));
}

void InProcessCommandBuffer::CancelAllQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  ScheduleGpuTask(
      base::BindOnce(&InProcessCommandBuffer::CancelAllQueriesOnGpuThread,
                     gpu_thread_weak_ptr_factory_.GetWeakPtr()));
}

void InProcessCommandBuffer::SignalQueryOnGpuThread(
    unsigned query_id,
    base::OnceClosure callback) {
  decoder_->SetQueryCallback(query_id, WrapClientCallback(std::move(callback)));
}

void InProcessCommandBuffer::CancelAllQueriesOnGpuThread() {
  decoder_->CancelAllQueries();
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

const gles2::FeatureInfo* InProcessCommandBuffer::GetFeatureInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  return context_group_->feature_info();
}

void InProcessCommandBuffer::HandleReturnDataOnOriginThread(
    std::vector<uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  if (gpu_control_client_) {
    gpu_control_client_->OnGpuControlReturnData(data);
  }
}

void InProcessCommandBuffer::UpdateActiveUrl() {
  if (!active_url_.is_empty())
    ContextUrl::SetActiveUrl(active_url_);
}

}  // namespace gpu
