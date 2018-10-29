// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gles2_command_buffer_stub.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/hash.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/swap_buffers_flags.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_workarounds.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

#if defined(OS_ANDROID)
#include "gpu/ipc/service/stream_texture_android.h"
#endif

namespace gpu {

GLES2CommandBufferStub::GLES2CommandBufferStub(
    GpuChannel* channel,
    const GPUCreateCommandBufferConfig& init_params,
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
      gles2_decoder_(nullptr),
      weak_ptr_factory_(this) {}

GLES2CommandBufferStub::~GLES2CommandBufferStub() {}

gpu::ContextResult GLES2CommandBufferStub::Initialize(
    CommandBufferStub* share_command_buffer_stub,
    const GPUCreateCommandBufferConfig& init_params,
    base::UnsafeSharedMemoryRegion shared_state_shm) {
  TRACE_EVENT0("gpu", "GLES2CommandBufferStub::Initialize");
  FastSetActiveURL(active_url_, active_url_hash_, channel_);

  GpuChannelManager* manager = channel_->gpu_channel_manager();
  DCHECK(manager);

  if (share_command_buffer_stub) {
    context_group_ = share_command_buffer_stub->context_group();
    DCHECK(context_group_->bind_generates_resource() ==
           init_params.attribs.bind_generates_resource);
  } else {
    scoped_refptr<gles2::FeatureInfo> feature_info = new gles2::FeatureInfo(
        manager->gpu_driver_bug_workarounds(), manager->gpu_feature_info());
    gpu::GpuMemoryBufferFactory* gmb_factory =
        manager->gpu_memory_buffer_factory();
    context_group_ = new gles2::ContextGroup(
        manager->gpu_preferences(), gles2::PassthroughCommandDecoderSupported(),
        manager->mailbox_manager(), CreateMemoryTracker(init_params),
        manager->shader_translator_cache(),
        manager->framebuffer_completeness_cache(), feature_info,
        init_params.attribs.bind_generates_resource, channel_->image_manager(),
        gmb_factory ? gmb_factory->AsImageFactory() : nullptr,
        manager->watchdog() /* progress_reporter */,
        manager->gpu_feature_info(), manager->discardable_manager(),
        manager->passthrough_discardable_manager(),
        manager->shared_image_manager());
  }

#if defined(OS_MACOSX)
  // Virtualize PreferIntegratedGpu contexts by default on OS X to prevent
  // performance regressions when enabling FCM.
  // http://crbug.com/180463
  if (init_params.attribs.gpu_preference == gl::PreferIntegratedGpu)
    use_virtualized_gl_context_ = true;
#endif

  use_virtualized_gl_context_ |=
      context_group_->feature_info()->workarounds().use_virtualized_gl_contexts;

  // MailboxManagerSync synchronization correctness currently depends on having
  // only a single context. See crbug.com/510243 for details.
  use_virtualized_gl_context_ |= manager->mailbox_manager()->UsesSync();

  bool offscreen = (surface_handle_ == kNullSurfaceHandle);
  gl::GLSurface* default_surface = manager->default_offscreen_surface();
  // On low-spec Android devices, the default offscreen surface is
  // RGB565, but WebGL rendering contexts still ask for RGBA8888 mode.
  // That combination works for offscreen rendering, we can still use
  // a virtualized context with the RGB565 backing surface since we're
  // not drawing to that. Explicitly set that as the desired surface
  // format to ensure it's treated as compatible where applicable.
  gl::GLSurfaceFormat surface_format =
      offscreen ? default_surface->GetFormat() : gl::GLSurfaceFormat();
#if defined(OS_ANDROID)
  if (init_params.attribs.red_size <= 5 &&
      init_params.attribs.green_size <= 6 &&
      init_params.attribs.blue_size <= 5 &&
      init_params.attribs.alpha_size == 0) {
    // We hit this code path when creating the onscreen render context
    // used for compositing on low-end Android devices.
    //
    // TODO(klausw): explicitly copy rgba sizes? Currently the formats
    // supported are only RGB565 and default (RGBA8888).
    surface_format.SetRGB565();
    DVLOG(1) << __FUNCTION__ << ": Choosing RGB565 mode.";
  }

  // We can only use virtualized contexts for onscreen command buffers if their
  // config is compatible with the offscreen ones - otherwise MakeCurrent fails.
  // Example use case is a client requesting an onscreen RGBA8888 buffer for
  // fullscreen video on a low-spec device with RGB565 default format.
  if (!surface_format.IsCompatible(default_surface->GetFormat()) && !offscreen)
    use_virtualized_gl_context_ = false;
#endif

  command_buffer_ = std::make_unique<CommandBufferService>(
      this, context_group_->transfer_buffer_manager());
  gles2_decoder_ = gles2::GLES2Decoder::Create(
      this, command_buffer_.get(), manager->outputter(), context_group_.get());
  set_decoder_context(std::unique_ptr<DecoderContext>(gles2_decoder_));

  sync_point_client_state_ =
      channel_->sync_point_manager()->CreateSyncPointClientState(
          CommandBufferNamespace::GPU_IO, command_buffer_id_, sequence_id_);

  if (offscreen) {
    // Do we want to create an offscreen rendering context suitable
    // for directly drawing to a separately supplied surface? In that
    // case, we must ensure that the surface used for context creation
    // is compatible with the requested attributes. This is explicitly
    // opt-in since some context such as for NaCl request custom
    // attributes but don't expect to get their own surface, and not
    // all surface factories support custom formats.
    if (init_params.attribs.own_offscreen_surface) {
      if (init_params.attribs.depth_size > 0) {
        surface_format.SetDepthBits(init_params.attribs.depth_size);
      }
      if (init_params.attribs.samples > 0) {
        surface_format.SetSamples(init_params.attribs.samples);
      }
      if (init_params.attribs.stencil_size > 0) {
        surface_format.SetStencilBits(init_params.attribs.stencil_size);
      }
      // Currently, we can't separately control alpha channel for surfaces,
      // it's generally enabled by default except for RGB565 and (on desktop)
      // smaller-than-32bit formats.
      //
      // TODO(klausw): use init_params.attribs.alpha_size here if possible.
    }
    if (!surface_format.IsCompatible(default_surface->GetFormat())) {
      DVLOG(1) << __FUNCTION__ << ": Hit the OwnOffscreenSurface path";
      use_virtualized_gl_context_ = false;
      surface_ = gl::init::CreateOffscreenGLSurfaceWithFormat(gfx::Size(),
                                                              surface_format);
      if (!surface_) {
        LOG(ERROR)
            << "ContextResult::kSurfaceFailure: Failed to create surface.";
        return gpu::ContextResult::kSurfaceFailure;
      }
    } else {
      surface_ = default_surface;
    }
  } else {
    switch (init_params.attribs.color_space) {
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
        weak_ptr_factory_.GetWeakPtr(), surface_handle_, surface_format);
    if (!surface_ || !surface_->Initialize(surface_format)) {
      surface_ = nullptr;
      LOG(ERROR) << "ContextResult::kSurfaceFailure: Failed to create surface.";
      return gpu::ContextResult::kSurfaceFailure;
    }
    if (init_params.attribs.enable_swap_timestamps_if_supported &&
        surface_->SupportsSwapTimestamps())
      surface_->SetEnableSwapTimestamps();
  }

  if (context_group_->use_passthrough_cmd_decoder()) {
    // When using the passthrough command decoder, only share with other
    // contexts in the explicitly requested share group
    if (share_command_buffer_stub) {
      share_group_ = share_command_buffer_stub->share_group();
    } else {
      share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
    }
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
    context = share_group_->GetSharedContext(surface_.get());
    if (!context) {
      context = gl::init::CreateGLContext(
          share_group_.get(), surface_.get(),
          GenerateGLContextAttribs(init_params.attribs, context_group_.get()));
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
      share_group_->SetSharedContext(surface_.get(), context.get());

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
                             GenerateGLContextAttribs(init_params.attribs,
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
        GenerateGLContextAttribs(init_params.attribs, context_group_.get()));
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

  if (!context->GetGLStateRestorer()) {
    context->SetGLStateRestorer(
        new GLStateRestorerImpl(gles2_decoder_->AsWeakPtr()));
  }

  if (!context_group_->has_program_cache() &&
      !context_group_->feature_info()->workarounds().disable_program_cache) {
    context_group_->set_program_cache(manager->program_cache());
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  auto result = gles2_decoder_->Initialize(surface_, context, offscreen,
                                           gpu::gles2::DisallowedFeatures(),
                                           init_params.attribs);
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

  if (offscreen && !active_url_.is_empty())
    manager->delegate()->DidCreateOffscreenContext(active_url_);

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

  manager->delegate()->DidCreateContextSuccessfully();
  initialized_ = true;
  return gpu::ContextResult::kSuccess;
}

#if defined(OS_WIN)
void GLES2CommandBufferStub::DidCreateAcceleratedSurfaceChildWindow(
    SurfaceHandle parent_window,
    SurfaceHandle child_window) {
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->delegate()->SendCreatedChildWindow(parent_window,
                                                          child_window);
}
#endif

void GLES2CommandBufferStub::DidSwapBuffersComplete(
    SwapBuffersCompleteParams params) {
  params.swap_response.swap_id = pending_swap_completed_params_.front().swap_id;
  pending_swap_completed_params_.pop_front();
  Send(new GpuCommandBufferMsg_SwapBuffersCompleted(route_id_, params));
}

const gles2::FeatureInfo* GLES2CommandBufferStub::GetFeatureInfo() const {
  return context_group_->feature_info();
}

const GpuPreferences& GLES2CommandBufferStub::GetGpuPreferences() const {
  return context_group_->gpu_preferences();
}

void GLES2CommandBufferStub::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  SwapBufferParams params = pending_presented_params_.front();
  pending_presented_params_.pop_front();

  if (params.flags & gpu::SwapBuffersFlags::kPresentationFeedback ||
      (params.flags & gpu::SwapBuffersFlags::kVSyncParams &&
       feedback.flags & gfx::PresentationFeedback::kVSync)) {
    Send(new GpuCommandBufferMsg_BufferPresented(route_id_, params.swap_id,
                                                 feedback));
  }
}

void GLES2CommandBufferStub::AddFilter(IPC::MessageFilter* message_filter) {
  return channel_->AddFilter(message_filter);
}

int32_t GLES2CommandBufferStub::GetRouteID() const {
  return route_id_;
}

void GLES2CommandBufferStub::OnTakeFrontBuffer(const Mailbox& mailbox) {
  TRACE_EVENT0("gpu", "CommandBufferStub::OnTakeFrontBuffer");
  if (!gles2_decoder_) {
    LOG(ERROR) << "Can't take front buffer before initialization.";
    return;
  }

  gles2_decoder_->TakeFrontBuffer(mailbox);
}

void GLES2CommandBufferStub::OnReturnFrontBuffer(const Mailbox& mailbox,
                                                 bool is_lost) {
  gles2_decoder_->ReturnFrontBuffer(mailbox, is_lost);
}

void GLES2CommandBufferStub::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {
  pending_swap_completed_params_.push_back({swap_id, flags});
  pending_presented_params_.push_back({swap_id, flags});
}

}  // namespace gpu
