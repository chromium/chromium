// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gles2_command_buffer_stub.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/json/json_writer.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/swap_buffers_flags.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "gpu/command_buffer/service/gpu_fence_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

namespace gpu {

GLES2CommandBufferStub::GLES2CommandBufferStub(
    GpuChannel* channel,
    const mojom::CreateCommandBufferParams& init_params,
    CommandBufferId command_buffer_id,
    SequenceId sequence_id,
    int32_t stream_id,
    int32_t route_id)
    : CommandBufferStub(channel,
                        init_params,
                        command_buffer_id,
                        sequence_id,
                        stream_id,
                        route_id),
      gles2_decoder_(nullptr) {}

GLES2CommandBufferStub::~GLES2CommandBufferStub() = default;

gpu::ContextResult GLES2CommandBufferStub::Initialize(
    const mojom::CreateCommandBufferParams& init_params,
    base::UnsafeSharedMemoryRegion shared_state_shm) {
  TRACE_EVENT0("gpu", "GLES2CommandBufferStub::Initialize");
  UpdateActiveUrl();

  const auto& attribs = *init_params.attribs->get_gles();

  GpuChannelManager* manager = channel_->gpu_channel_manager();
  DCHECK(manager);
  memory_tracker_ = CreateMemoryTracker();

  auto feature_info = base::MakeRefCounted<gles2::FeatureInfo>(
      manager->gpu_driver_bug_workarounds(), manager->gpu_feature_info());
  context_group_ = base::MakeRefCounted<gles2::ContextGroup>(
      manager->gpu_preferences(), CreateMemoryTracker(),
      manager->shader_translator_cache(),
      manager->framebuffer_completeness_cache(), feature_info,
      manager->watchdog() /* progress_reporter */, manager->gpu_feature_info(),
      manager->shared_image_manager());

  // If the `fail_if_major_perf_caveat` context creation attribute was true
  // and we are using a software renderer, fail.
  if (attribs.fail_if_major_perf_caveat &&
      context_group_->feature_info()->feature_flags().is_software_webgl) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "fail_if_major_perf_caveat + software gl";
    return gpu::ContextResult::kFatalFailure;
  }

#if BUILDFLAG(IS_MAC)
  // Virtualize GpuPreference::kLowPower contexts by default on OS X to prevent
  // performance regressions when enabling FCM.
  // http://crbug.com/180463
  if (attribs.gpu_preference == gl::GpuPreference::kLowPower) {
    use_virtualized_gl_context_ = true;
  }
#endif

  use_virtualized_gl_context_ |=
      context_group_->feature_info()->workarounds().use_virtualized_gl_contexts;

  command_buffer_ = std::make_unique<CommandBufferService>(
      this, context_group_->memory_tracker());
  auto decoder = gles2::GLES2Decoder::Create(
      this, command_buffer_.get(), manager->outputter(), context_group_.get());
  gles2_decoder_ = decoder.get();
  set_decoder_context(std::move(decoder));

  scoped_sync_point_client_state_ =
      channel_->scheduler()->CreateSyncPointClientState(
          sequence_id_, CommandBufferNamespace::GPU_IO, command_buffer_id_);

  // TODO(crbug.com/40198488): Remove this after testing.
  // Only enable multiple displays on ANGLE/Metal and only behind a feature.
  bool force_default_display = true;
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal &&
      features::SupportsEGLDualGPURendering()) {
    force_default_display = false;
  }
  gl::GpuPreference gpu_preference = attribs.gpu_preference;
  // If the user queries a low-power context, it's better to use whatever the
  // default GPU used by Chrome is, which may be different than the low-power
  // GPU determined by GLDisplayManager.
  if (gpu_preference == gl::GpuPreference::kLowPower ||
      gpu_preference == gl::GpuPreference::kNone || force_default_display) {
    gpu_preference = gl::GpuPreference::kDefault;
  }

  // Query and initialize the default display for this GPU preference,
  // ignoring any queried display key for now. For simplicity we need
  // to initialize the default display per-GPU first.
  // We may be requesting a new GPU/display, so get or initialize the display.
  gl::GLDisplay* display =
      gl::init::GetOrInitializeGLOneOffPlatformImplementation(
          /*fallback_to_software_gl=*/false, /*disable_gl_drawing=*/false,
          /*init_extensions=*/true,
          /*gpu_preference=*/gpu_preference);

  // If the user queries a key to create a distinct display on this GPU,
  // check if this display already exists, and if not, initialize it from
  // the default display on this GPU.
  gl::DisplayKey display_key = gl::DisplayKey::kDefault;
  if (manager->gpu_preferences().force_separate_egl_display_for_webgl_testing &&
      features::SupportsEGLDualGPURendering()) {
    display_key = gl::DisplayKey::kSeparateEGLDisplayForWebGLTesting;
  }

  if (display_key != gl::DisplayKey::kDefault) {
    gl::GLDisplay* keyed_display = gl::GetDisplay(gpu_preference, display_key);
    if (!keyed_display->IsInitialized()) {
      keyed_display->Initialize(display);
    }
    display = keyed_display;
  }

  gl::GLSurface* default_surface = manager->default_offscreen_surface();
  if (default_surface->GetGLDisplay() == display) {
    surface_ = default_surface;
  } else {
    // The default surface was created on a different display, create a
    // new surface on the requested display.
    surface_ = gl::init::CreateOffscreenGLSurface(display, gfx::Size());
  }

  if (context_group_->use_passthrough_cmd_decoder()) {
    // Virtualized contexts don't work with passthrough command decoder.
    // See https://crbug.com/914976
    use_virtualized_gl_context_ = false;
    // When using the passthrough command decoder, only share with other
    // contexts in the explicitly requested share group
    share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
  } else {
    // When using the validating command decoder, always use the global share
    // group
    share_group_ = channel_->share_group();
  }

  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");

  scoped_refptr<gl::GLContext> context;
  if (use_virtualized_gl_context_ && share_group_) {
    context = share_group_->shared_context();
    if (context && (!context->MakeCurrent(surface_.get()) ||
                    context->CheckStickyGraphicsResetStatus() != GL_NO_ERROR)) {
      context = nullptr;
    }
    if (!context) {
      context = gl::init::CreateGLContext(
          share_group_.get(), surface_.get(),
          GenerateGLContextAttribsForDecoder(attribs.context_type,
                                             attribs.gpu_preference,
                                             context_group_.get()));
      if (!context) {
        // TODO(piman): This might not be fatal, we could recurse into
        // CreateGLContext to get more info, tho it should be exceedingly
        // rare and may not be recoverable anyway.
        LOG(ERROR) << "ContextResult::kFatalFailure: "
                      "Failed to create shared context for virtualization.";
        return gpu::ContextResult::kFatalFailure;
      }
      // Ensure that context creation did not lose track of the intended share
      // group.
      DCHECK(context->share_group() == share_group_.get());
      share_group_->SetSharedContext(context.get());

      // This needs to be called against the real shared context, not the
      // virtual context created below.
      manager->gpu_feature_info().ApplyToGLContext(context.get());
    }
    // This should be either:
    // (1) a non-virtual GL context, or
    // (2) a mock/stub context.
    DCHECK(context->GetHandle() ||
           gl::GetGLImplementation() == gl::kGLImplementationMockGL ||
           gl::GetGLImplementation() == gl::kGLImplementationStubGL);
    context = base::MakeRefCounted<GLContextVirtual>(
        share_group_.get(), context.get(), gles2_decoder_->AsWeakPtr());
    if (!context->Initialize(surface_.get(),
                             GenerateGLContextAttribsForDecoder(
                                 attribs.context_type, attribs.gpu_preference,
                                 context_group_.get()))) {
      // The real context created above for the default offscreen surface
      // might not be compatible with this surface.
      context = nullptr;
      // TODO(piman): This might not be fatal, we could recurse into
      // CreateGLContext to get more info, tho it should be exceedingly
      // rare and may not be recoverable anyway.
      LOG(ERROR) << "ContextResult::kFatalFailure: "
                    "Failed to initialize virtual GL context.";
      return gpu::ContextResult::kFatalFailure;
    }
  } else {
    context = gl::init::CreateGLContext(
        share_group_.get(), surface_.get(),
        GenerateGLContextAttribsForDecoder(attribs.context_type,
                                           attribs.gpu_preference,
                                           context_group_.get()));
    if (!context) {
      // TODO(piman): This might not be fatal, we could recurse into
      // CreateGLContext to get more info, tho it should be exceedingly
      // rare and may not be recoverable anyway.
      LOG(ERROR) << "ContextResult::kFatalFailure: Failed to create context.";
      return gpu::ContextResult::kFatalFailure;
    }

    manager->gpu_feature_info().ApplyToGLContext(context.get());
  }

  if (!context->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "ContextResult::kTransientFailure: "
                  "Failed to make context current.";
    return gpu::ContextResult::kTransientFailure;
  }

  // The GLStateRestorer is not used with the passthrough command decoder
  // because not all state is tracked in the decoder. Virtualized contexts are
  // also not used.
  if (!context->GetGLStateRestorer() &&
      !context_group_->use_passthrough_cmd_decoder()) {
    context->SetGLStateRestorer(
        new GLStateRestorerImpl(gles2_decoder_->AsWeakPtr()));
  }

  if (!context_group_->has_program_cache() &&
      !context_group_->feature_info()->workarounds().disable_program_cache) {
    context_group_->set_program_cache(manager->program_cache());
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  auto result = gles2_decoder_->Initialize(
      surface_, context, /*offscreen=*/true, attribs.context_type,
      attribs.lose_context_when_out_of_memory);
  if (result != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to initialize decoder.";
    return result;
  }

  if (manager->gpu_preferences().enable_gpu_service_logging) {
    gles2_decoder_->SetLogCommands(true);
  }

  const size_t kSharedStateSize = sizeof(CommandBufferSharedState);
  base::WritableSharedMemoryMapping shared_state_mapping =
      shared_state_shm.MapAt(0, kSharedStateSize);
  if (!shared_state_mapping.IsValid()) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "Failed to map shared state buffer.";
    return gpu::ContextResult::kFatalFailure;
  }
  command_buffer_->SetSharedStateBuffer(MakeBackingFromSharedMemory(
      std::move(shared_state_shm), std::move(shared_state_mapping)));

  if (!active_url_.is_empty()) {
    manager->delegate()->DidCreateOffscreenContext(active_url_.url());
  }

  if (use_virtualized_gl_context_) {
    // If virtualized GL contexts are in use, then real GL context state
    // is in an indeterminate state, since the GLStateRestorer was not
    // initialized at the time the GLContextVirtual was made current. In
    // the case that this command decoder is the next one to be
    // processed, force a "full virtual" MakeCurrent to be performed.
    // Note that GpuChannel's initialization of the gpu::Capabilities
    // expects the context to be left current.
    context->ForceReleaseVirtuallyCurrent();
    if (!context->MakeCurrent(surface_.get())) {
      LOG(ERROR) << "ContextResult::kTransientFailure: "
                    "Failed to make context current after initialization.";
      return gpu::ContextResult::kTransientFailure;
    }
  }

  if (IsWebGLContextType(attribs.context_type)) {
    gl::GLDisplayEGL* display_egl = display->GetAs<gl::GLDisplayEGL>();
    if (display_egl) {
      UMA_HISTOGRAM_ENUMERATION("GPU.WebGLDisplayType",
                                display_egl->GetDisplayType(),
                                gl::DISPLAY_TYPE_MAX);
    }
  }

  manager->delegate()->DidCreateContextSuccessfully();
  initialized_ = true;
  return gpu::ContextResult::kSuccess;
}

MemoryTracker* GLES2CommandBufferStub::GetContextGroupMemoryTracker() const {
  return context_group_->memory_tracker();
}

base::WeakPtr<CommandBufferStub> GLES2CommandBufferStub::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GLES2CommandBufferStub::OnGpuSwitched() {
  client().OnGpuSwitched();
}

void GLES2CommandBufferStub::CreateGpuFenceFromHandle(
    uint32_t gpu_fence_id,
    gfx::GpuFenceHandle handle) {
  ScopedContextOperation operation(*this);
  if (!operation.is_context_current())
    return;

  if (!context_group_->feature_info()->feature_flags().chromium_gpu_fence) {
    DLOG(ERROR) << "CHROMIUM_gpu_fence unavailable";
    command_buffer_->SetParseError(error::kLostContext);
    return;
  }

  if (gles2_decoder_->GetGpuFenceManager()->CreateGpuFenceFromHandle(
          gpu_fence_id, std::move(handle)))
    return;

  // The insertion failed. This shouldn't happen, force context loss to avoid
  // inconsistent state.
  command_buffer_->SetParseError(error::kLostContext);
  CheckContextLost();
}

void GLES2CommandBufferStub::GetGpuFenceHandle(
    uint32_t gpu_fence_id,
    GetGpuFenceHandleCallback callback) {
  ScopedContextOperation operation(*this);
  if (!operation.is_context_current())
    return;

  if (!context_group_->feature_info()->feature_flags().chromium_gpu_fence) {
    DLOG(ERROR) << "CHROMIUM_gpu_fence unavailable";
    command_buffer_->SetParseError(error::kLostContext);
    return;
  }

  auto* manager = gles2_decoder_->GetGpuFenceManager();
  gfx::GpuFenceHandle handle;
  if (manager->IsValidGpuFence(gpu_fence_id)) {
    std::unique_ptr<gfx::GpuFence> gpu_fence =
        manager->GetGpuFence(gpu_fence_id);
    handle = gpu_fence->GetGpuFenceHandle().Clone();
  } else {
    // Retrieval failed. This shouldn't happen, force context loss to avoid
    // inconsistent state.
    DLOG(ERROR) << "GpuFence not found";
    command_buffer_->SetParseError(error::kLostContext);
    CheckContextLost();
  }

  std::move(callback).Run(std::move(handle));
}

}  // namespace gpu
