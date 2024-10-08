// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/raster_command_buffer_stub.h"

#include <memory>
#include <utility>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/raster_decoder.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "gpu/ipc/service/stream_texture_android.h"
#endif

namespace gpu {

RasterCommandBufferStub::RasterCommandBufferStub(
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
                        route_id) {}

RasterCommandBufferStub::~RasterCommandBufferStub() {}

gpu::ContextResult RasterCommandBufferStub::Initialize(
    CommandBufferStub* share_command_buffer_stub,
    const mojom::CreateCommandBufferParams& init_params,
    base::UnsafeSharedMemoryRegion shared_state_shm) {
  TRACE_EVENT0("gpu", "RasterBufferStub::Initialize");
  UpdateActiveUrl();

  GpuChannelManager* manager = channel_->gpu_channel_manager();
  DCHECK(manager);

  if (share_command_buffer_stub) {
    LOG(ERROR) << "Using a share group is not supported with RasterDecoder";
    return ContextResult::kFatalFailure;
  }

  if (init_params.attribs.gpu_preference != gl::GpuPreference::kLowPower ||
      init_params.attribs.context_type != CONTEXT_TYPE_OPENGLES2 ||
      init_params.attribs.bind_generates_resource) {
    LOG(ERROR) << "ContextResult::kFatalFailure: Incompatible creation attribs "
                  "used with RasterDecoder";
    return ContextResult::kFatalFailure;
  }

  ContextResult result;
  auto shared_context_state = manager->GetSharedContextState(&result);
  if (!shared_context_state) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "Failed to create raster decoder state.";
    DCHECK_NE(result, gpu::ContextResult::kSuccess);
    return result;
  }

  DCHECK(shared_context_state->IsGLInitialized());

  surface_ = shared_context_state->surface();
  share_group_ = shared_context_state->share_group();
  use_virtualized_gl_context_ =
      shared_context_state->use_virtualized_gl_contexts();

  memory_tracker_ = CreateMemoryTracker();

  command_buffer_ =
      std::make_unique<CommandBufferService>(this, memory_tracker_.get());
  std::unique_ptr<raster::RasterDecoder> decoder(raster::RasterDecoder::Create(
      this, command_buffer_.get(), manager->outputter(),
      manager->gpu_feature_info(), manager->gpu_preferences(),
      memory_tracker_.get(), manager->shared_image_manager(),
      shared_context_state, channel()->is_gpu_host()));

  scoped_sync_point_client_state_ =
      channel_->scheduler()->CreateSyncPointClientState(
          sequence_id_, CommandBufferNamespace::GPU_IO, command_buffer_id_);

  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");

  scoped_refptr<gl::GLContext> context = shared_context_state->context();
  if (!shared_context_state->MakeCurrent(nullptr, false /* needs_gl */)) {
    LOG(ERROR) << "ContextResult::kTransientFailure: "
                  "Failed to make context current.";
    return gpu::ContextResult::kTransientFailure;
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  result = decoder->Initialize(surface_, context, true /* offscreen */,
                               gpu::gles2::DisallowedFeatures(),
                               init_params.attribs);
  if (result != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to initialize decoder.";
    return result;
  }

  if (manager->gpu_preferences().enable_gpu_service_logging) {
    decoder->SetLogCommands(true);
  }
  set_decoder_context(std::move(decoder));

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

  if (!active_url_.is_empty())
    manager->delegate()->DidCreateOffscreenContext(active_url_.url());

  manager->delegate()->DidCreateContextSuccessfully();
  initialized_ = true;
  return gpu::ContextResult::kSuccess;
}

MemoryTracker* RasterCommandBufferStub::GetContextGroupMemoryTracker() const {
  return nullptr;
}

base::WeakPtr<CommandBufferStub> RasterCommandBufferStub::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void RasterCommandBufferStub::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {}

void RasterCommandBufferStub::SetActiveURL(GURL url) {
  active_url_ = ContextUrl(std::move(url));
  UpdateActiveUrl();
}

}  // namespace gpu
