// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/webgpu_command_buffer_stub.h"

#include <memory>
#include <utility>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/dawn_caching_interface.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
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

WebGPUCommandBufferStub::WebGPUCommandBufferStub(
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

WebGPUCommandBufferStub::~WebGPUCommandBufferStub() {
  // Must run before memory_tracker_ is destroyed.
  decoder_context()->Destroy(false);

  memory_tracker_ = nullptr;
}

gpu::ContextResult WebGPUCommandBufferStub::Initialize(
    CommandBufferStub* share_command_buffer_stub,
    const mojom::CreateCommandBufferParams& init_params,
    base::UnsafeSharedMemoryRegion shared_state_shm) {
#if BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40513405): Implement this.
  NOTIMPLEMENTED();
  LOG(ERROR) << "ContextResult::kFatalFailure: no fuchsia support";
  return gpu::ContextResult::kFatalFailure;
#else
  TRACE_EVENT0("gpu", "WebGPUBufferStub::Initialize");
  UpdateActiveUrl();

  GpuChannelManager* manager = channel_->gpu_channel_manager();
  DCHECK(manager);

  if (share_command_buffer_stub) {
    LOG(ERROR) << "Using a share group is not supported with WebGPUDecoder";
    return ContextResult::kFatalFailure;
  }

  if (init_params.attribs.context_type != CONTEXT_TYPE_WEBGPU) {
    LOG(ERROR) << "ContextResult::kFatalFailure: Incompatible creation attribs "
                  "used with WebGPUDecoder";
    return ContextResult::kFatalFailure;
  }

  ContextResult result;
  scoped_refptr<SharedContextState> shared_context_state =
      manager->GetSharedContextState(&result);
  if (!shared_context_state) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "Failed to create WebGPU decoder state.";
    DCHECK_NE(result, gpu::ContextResult::kSuccess);
    return result;
  }

  share_group_ = manager->share_group();
  use_virtualized_gl_context_ = false;

  memory_tracker_ = CreateMemoryTracker();

  webgpu::DawnCacheOptions dawn_cache_options = {
      manager->dawn_caching_interface_factory(),
      channel_->GetCacheHandleForType(gpu::GpuDiskCacheType::kDawnWebGPU)};

  command_buffer_ =
      std::make_unique<CommandBufferService>(this, memory_tracker_.get());
  std::unique_ptr<webgpu::WebGPUDecoder> decoder(webgpu::WebGPUDecoder::Create(
      this, command_buffer_.get(), manager->shared_image_manager(),
      memory_tracker_.get(), manager->outputter(), manager->gpu_preferences(),
      std::move(shared_context_state), dawn_cache_options, channel_));

  scoped_sync_point_client_state_ =
      channel_->scheduler()->CreateSyncPointClientState(
          sequence_id_, CommandBufferNamespace::GPU_IO, command_buffer_id_);

  result = decoder->Initialize(manager->gpu_feature_info());
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
#endif  // BUILDFLAG(IS_FUCHSIA)
}

MemoryTracker* WebGPUCommandBufferStub::GetContextGroupMemoryTracker() const {
  return nullptr;
}

base::WeakPtr<CommandBufferStub> WebGPUCommandBufferStub::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebGPUCommandBufferStub::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {}

}  // namespace gpu
