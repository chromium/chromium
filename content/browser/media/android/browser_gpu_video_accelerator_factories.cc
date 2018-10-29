// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_gpu_video_accelerator_factories.h"

#include "content/browser/browser_main_loop.h"
#include "content/public/browser/android/gpu_video_accelerator_factories_provider.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/ipc/common/media_messages.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

namespace {

void OnGpuChannelEstablished(
    GpuVideoAcceleratorFactoriesCallback callback,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.red_size = 8;
  attributes.green_size = 8;
  attributes.blue_size = 8;
  attributes.stencil_size = 0;
  attributes.depth_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;

  gpu::GpuChannelEstablishFactory* factory =
      BrowserMainLoop::GetInstance()->gpu_channel_establish_factory();

  int32_t stream_id = kGpuStreamIdDefault;
  gpu::SchedulingPriority stream_priority = kGpuStreamPriorityUI;

  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;
  constexpr bool support_grcontext = true;

  auto context_provider =
      base::MakeRefCounted<ws::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), factory->GetGpuMemoryBufferManager(),
          stream_id, stream_priority, gpu::kNullSurfaceHandle,
          GURL(std::string("chrome://gpu/"
                           "BrowserGpuVideoAcceleratorFactories::"
                           "CreateGpuVideoAcceleratorFactories")),
          automatic_flushes, support_locking, support_grcontext,
          gpu::SharedMemoryLimits::ForMailboxContext(), attributes,
          ws::command_buffer_metrics::ContextType::UNKNOWN);

  // TODO(xingliu): This is on main thread, move to another thread?
  context_provider->BindToCurrentThread();

  auto gpu_factories = std::make_unique<BrowserGpuVideoAcceleratorFactories>(
      std::move(context_provider));
  std::move(callback).Run(std::move(gpu_factories));
}

}  // namespace

void CreateGpuVideoAcceleratorFactories(
    GpuVideoAcceleratorFactoriesCallback callback) {
  BrowserMainLoop::GetInstance()
      ->gpu_channel_establish_factory()
      ->EstablishGpuChannel(
          base::BindOnce(&OnGpuChannelEstablished, std::move(callback)));
}

BrowserGpuVideoAcceleratorFactories::BrowserGpuVideoAcceleratorFactories(
    scoped_refptr<ws::ContextProviderCommandBuffer> context_provider)
    : context_provider_(std::move(context_provider)) {}

BrowserGpuVideoAcceleratorFactories::~BrowserGpuVideoAcceleratorFactories() =
    default;

bool BrowserGpuVideoAcceleratorFactories::IsGpuVideoAcceleratorEnabled() {
  return false;
}

base::UnguessableToken BrowserGpuVideoAcceleratorFactories::GetChannelToken() {
  if (channel_token_.is_empty()) {
    context_provider_->GetCommandBufferProxy()->channel()->Send(
        new GpuCommandBufferMsg_GetChannelToken(&channel_token_));
  }

  return channel_token_;
}

int32_t BrowserGpuVideoAcceleratorFactories::GetCommandBufferRouteId() {
  return context_provider_->GetCommandBufferProxy()->route_id();
}

bool BrowserGpuVideoAcceleratorFactories::IsDecoderConfigSupported(
    const media::VideoDecoderConfig& config) {
  // TODO(sandersd): Add a cache here too?
  return true;
}

std::unique_ptr<media::VideoDecoder>
BrowserGpuVideoAcceleratorFactories::CreateVideoDecoder(
    media::MediaLog* media_log,
    const media::RequestOverlayInfoCB& request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  return nullptr;
}

std::unique_ptr<media::VideoDecodeAccelerator>
BrowserGpuVideoAcceleratorFactories::CreateVideoDecodeAccelerator() {
  return nullptr;
}

std::unique_ptr<media::VideoEncodeAccelerator>
BrowserGpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator() {
  return nullptr;
}

bool BrowserGpuVideoAcceleratorFactories::CreateTextures(
    int32_t count,
    const gfx::Size& size,
    std::vector<uint32_t>* texture_ids,
    std::vector<gpu::Mailbox>* texture_mailboxes,
    uint32_t texture_target) {
  return false;
}

void BrowserGpuVideoAcceleratorFactories::DeleteTexture(uint32_t texture_id) {}

gpu::SyncToken BrowserGpuVideoAcceleratorFactories::CreateSyncToken() {
  return gpu::SyncToken();
}

void BrowserGpuVideoAcceleratorFactories::ShallowFlushCHROMIUM() {}

void BrowserGpuVideoAcceleratorFactories::WaitSyncToken(
    const gpu::SyncToken& sync_token) {}

void BrowserGpuVideoAcceleratorFactories::SignalSyncToken(
    const gpu::SyncToken& sync_token,
    base::OnceClosure callback) {}

std::unique_ptr<gfx::GpuMemoryBuffer>
BrowserGpuVideoAcceleratorFactories::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return nullptr;
}

bool BrowserGpuVideoAcceleratorFactories::
    ShouldUseGpuMemoryBuffersForVideoFrames(bool for_media_stream) const {
  return false;
}

unsigned BrowserGpuVideoAcceleratorFactories::ImageTextureTarget(
    gfx::BufferFormat format) {
  return -1;
}

media::GpuVideoAcceleratorFactories::OutputFormat
BrowserGpuVideoAcceleratorFactories::VideoFrameOutputFormat(
    media::VideoPixelFormat pixel_format) {
  return GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED;
}

gpu::gles2::GLES2Interface* BrowserGpuVideoAcceleratorFactories::ContextGL() {
  return nullptr;
}

std::unique_ptr<base::SharedMemory>
BrowserGpuVideoAcceleratorFactories::CreateSharedMemory(size_t size) {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserGpuVideoAcceleratorFactories::GetTaskRunner() {
  return nullptr;
}

media::VideoDecodeAccelerator::Capabilities
BrowserGpuVideoAcceleratorFactories::GetVideoDecodeAcceleratorCapabilities() {
  DCHECK(context_provider_);
  auto* proxy = context_provider_->GetCommandBufferProxy();
  DCHECK(proxy);
  DCHECK(proxy->channel());

  return media::GpuVideoAcceleratorUtil::ConvertGpuToMediaDecodeCapabilities(
      proxy->channel()->gpu_info().video_decode_accelerator_capabilities);
}

media::VideoEncodeAccelerator::SupportedProfiles
BrowserGpuVideoAcceleratorFactories::
    GetVideoEncodeAcceleratorSupportedProfiles() {
  return media::VideoEncodeAccelerator::SupportedProfiles();
}

scoped_refptr<ws::ContextProviderCommandBuffer>
BrowserGpuVideoAcceleratorFactories::GetMediaContextProvider() {
  return context_provider_;
}

void BrowserGpuVideoAcceleratorFactories::SetRenderingColorSpace(
    const gfx::ColorSpace& color_space) {}

}  // namespace content
