// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_gpu_video_accelerator_factories.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/browser_main_loop.h"
#include "content/public/browser/android/gpu_video_accelerator_factories_provider.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

namespace {

void OnGpuChannelEstablished(
    GpuVideoAcceleratorFactoriesCallback callback,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  gpu::ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;
  attributes.enable_raster_interface = true;
  attributes.enable_oop_rasterization = true;
  attributes.enable_gles2_interface = false;
  attributes.enable_grcontext = false;

  int32_t stream_id = kGpuStreamIdDefault;
  gpu::SchedulingPriority stream_priority = kGpuStreamPriorityUI;

  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;

  auto context_provider =
      base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), stream_id, stream_priority,
          gpu::kNullSurfaceHandle,
          GURL(std::string("chrome://gpu/"
                           "BrowserGpuVideoAcceleratorFactories::"
                           "CreateGpuVideoAcceleratorFactories")),
          automatic_flushes, support_locking,
          gpu::SharedMemoryLimits::ForMailboxContext(), attributes,
          viz::command_buffer_metrics::ContextType::UNKNOWN);
  context_provider->BindToCurrentSequence();

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
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider)
    : context_provider_(std::move(context_provider)) {}

BrowserGpuVideoAcceleratorFactories::~BrowserGpuVideoAcceleratorFactories() =
    default;

bool BrowserGpuVideoAcceleratorFactories::IsGpuVideoDecodeAcceleratorEnabled() {
  return false;
}

bool BrowserGpuVideoAcceleratorFactories::IsGpuVideoEncodeAcceleratorEnabled() {
  return false;
}

void BrowserGpuVideoAcceleratorFactories::GetChannelToken(
    gpu::mojom::GpuChannel::GetChannelTokenCallback cb) {
  DCHECK(cb);
  if (!channel_token_.is_empty()) {
    // Use cached token.
    std::move(cb).Run(channel_token_);
    return;
  }

  // Retrieve a channel token if needed.
  bool request_channel_token = channel_token_callbacks_.empty();
  channel_token_callbacks_.AddUnsafe(std::move(cb));
  if (request_channel_token) {
    context_provider_->GetCommandBufferProxy()->GetGpuChannel().GetChannelToken(
        base::BindOnce(
            &BrowserGpuVideoAcceleratorFactories::OnChannelTokenReady,
            base::Unretained(this)));
  }
}

void BrowserGpuVideoAcceleratorFactories::OnChannelTokenReady(
    const base::UnguessableToken& token) {
  channel_token_ = token;
  channel_token_callbacks_.Notify(channel_token_);
  DCHECK(channel_token_callbacks_.empty());
}

int32_t BrowserGpuVideoAcceleratorFactories::GetCommandBufferRouteId() {
  return context_provider_->GetCommandBufferProxy()->route_id();
}

media::GpuVideoAcceleratorFactories::Supported
BrowserGpuVideoAcceleratorFactories::IsDecoderConfigSupported(
    const media::VideoDecoderConfig& config) {
  // Tell the caller to just try it, there are no other decoders to fall back on
  // anyway.
  return media::GpuVideoAcceleratorFactories::Supported::kTrue;
}

media::VideoDecoderType BrowserGpuVideoAcceleratorFactories::GetDecoderType() {
  return media::VideoDecoderType::kMediaCodec;
}

bool BrowserGpuVideoAcceleratorFactories::IsDecoderSupportKnown() {
  return true;
}

void BrowserGpuVideoAcceleratorFactories::NotifyDecoderSupportKnown(
    base::OnceClosure callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

std::unique_ptr<media::VideoDecoder>
BrowserGpuVideoAcceleratorFactories::CreateVideoDecoder(
    media::MediaLog* media_log,
    media::RequestOverlayInfoCB request_overlay_info_cb) {
  return nullptr;
}

std::unique_ptr<media::VideoEncodeAccelerator>
BrowserGpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator() {
  return nullptr;
}

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

media::GpuVideoAcceleratorFactories::OutputFormat
BrowserGpuVideoAcceleratorFactories::VideoFrameOutputFormat(
    media::VideoPixelFormat pixel_format) {
  return GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED;
}

gpu::SharedImageInterface*
BrowserGpuVideoAcceleratorFactories::SharedImageInterface() {
  NOTREACHED();
}

gpu::GpuMemoryBufferManager*
BrowserGpuVideoAcceleratorFactories::GpuMemoryBufferManager() {
  NOTREACHED();
}

base::UnsafeSharedMemoryRegion
BrowserGpuVideoAcceleratorFactories::CreateSharedMemoryRegion(size_t size) {
  return {};
}

scoped_refptr<base::SequencedTaskRunner>
BrowserGpuVideoAcceleratorFactories::GetTaskRunner() {
  return nullptr;
}

std::optional<media::VideoEncodeAccelerator::SupportedProfiles>
BrowserGpuVideoAcceleratorFactories::
    GetVideoEncodeAcceleratorSupportedProfiles() {
  return media::VideoEncodeAccelerator::SupportedProfiles();
}

bool BrowserGpuVideoAcceleratorFactories::IsEncoderSupportKnown() {
  return true;
}

void BrowserGpuVideoAcceleratorFactories::NotifyEncoderSupportKnown(
    base::OnceClosure callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

viz::RasterContextProvider*
BrowserGpuVideoAcceleratorFactories::GetMediaContextProvider() {
  return context_provider_.get();
}

const gpu::Capabilities*
BrowserGpuVideoAcceleratorFactories::ContextCapabilities() {
  return context_provider_ ? &(context_provider_->ContextCapabilities())
                           : nullptr;
}

void BrowserGpuVideoAcceleratorFactories::SetRenderingColorSpace(
    const gfx::ColorSpace& color_space) {}

const gfx::ColorSpace&
BrowserGpuVideoAcceleratorFactories::GetRenderingColorSpace() const {
  static constexpr gfx::ColorSpace cs = gfx::ColorSpace::CreateSRGB();
  return cs;
}

}  // namespace content
