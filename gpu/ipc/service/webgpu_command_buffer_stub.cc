// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/webgpu_command_buffer_stub.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
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

WebGPUCommandBufferStub::WebGPUCommandBufferStub(
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
                        route_id) {}

WebGPUCommandBufferStub::~WebGPUCommandBufferStub() {}

gpu::ContextResult WebGPUCommandBufferStub::Initialize(
    CommandBufferStub* share_command_buffer_stub,
    const GPUCreateCommandBufferConfig& init_params,
    base::UnsafeSharedMemoryRegion shared_state_shm) {
#if defined(OS_FUCHSIA)
  // TODO(crbug.com/707031): Implement this.
  NOTIMPLEMENTED();
  LOG(ERROR) << "ContextResult::kFatalFailure: no fuchsia support";
  return gpu::ContextResult::kFatalFailure;
#else
  TRACE_EVENT0("gpu", "WebGPUBufferStub::Initialize");
  FastSetActiveURL(active_url_, active_url_hash_, channel_);

  GpuChannelManager* manager = channel_->gpu_channel_manager();
  DCHECK(manager);

  if (share_command_buffer_stub) {
    LOG(ERROR) << "Using a share group is not supported with WebGPUDecoder";
    return ContextResult::kFatalFailure;
  }

  if (surface_handle_ != kNullSurfaceHandle) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "WebGPUInterface clients must render offscreen.";
    return ContextResult::kFatalFailure;
  }

  if (init_params.attribs.context_type != CONTEXT_TYPE_WEBGPU) {
    LOG(ERROR) << "ContextResult::kFatalFailure: Incompatible creation attribs "
                  "used with WebGPUDecoder";
    return ContextResult::kFatalFailure;
  }

  share_group_ = manager->share_group();
  use_virtualized_gl_context_ = false;

  TransferBufferManager* transfer_buffer_manager;
  // TODO: all of this is necessary to get a transfer buffer manager - we would
  // prefer to create a standalone one instead.
  {
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

    transfer_buffer_manager = context_group_->transfer_buffer_manager();
  }

  command_buffer_ =
      std::make_unique<CommandBufferService>(this, transfer_buffer_manager);
  std::unique_ptr<webgpu::WebGPUDecoder> decoder(webgpu::WebGPUDecoder::Create(
      this, command_buffer_.get(), manager->outputter()));

  sync_point_client_state_ =
      channel_->sync_point_manager()->CreateSyncPointClientState(
          CommandBufferNamespace::GPU_IO, command_buffer_id_, sequence_id_);

  // Initialize the decoder with either the view or pbuffer GLContext.
  ContextResult result = decoder->Initialize(
      nullptr, nullptr, true /* offscreen */, gpu::gles2::DisallowedFeatures(),
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
    manager->delegate()->DidCreateOffscreenContext(active_url_);

  manager->delegate()->DidCreateContextSuccessfully();
  initialized_ = true;
  return gpu::ContextResult::kSuccess;
#endif  // defined(OS_FUCHSIA)
}

// WebGPUInterface clients should not manipulate the front buffer.
void WebGPUCommandBufferStub::OnTakeFrontBuffer(const Mailbox& mailbox) {
  LOG(ERROR) << "Called WebGPUCommandBufferStub::OnTakeFrontBuffer";
}
void WebGPUCommandBufferStub::OnReturnFrontBuffer(const Mailbox& mailbox,
                                                  bool is_lost) {
  LOG(ERROR) << "Called WebGPUCommandBufferStub::OnReturnFrontBuffer";
}

void WebGPUCommandBufferStub::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {}

}  // namespace gpu
