// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/gpu/gpu_video_accelerator_factories_impl.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/content_features.h"
#include "content/renderer/media/codec_factory.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/decoder.h"
#include "media/base/media_switches.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/mojo/buildflags.h"
#include "media/video/video_encode_accelerator.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace content {

#if BUILDFLAG(IS_WIN)
namespace {

// Use NV12 as the default video frame output format. Note that NV12 is the
// preferred 4:2:0 pixel format on Windows according to:
// https://learn.microsoft.com/en-us/windows-hardware/drivers/display/4-2-0-video-pixel-formats
// https://learn.microsoft.com/en-us/windows/win32/medfound/recommended-8-bit-yuv-formats-for-video-rendering#nv12
BASE_FEATURE(kUseNV12OutputFormat,
             "UseNV12OutputFormat",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace
#endif

// static
std::unique_ptr<GpuVideoAcceleratorFactoriesImpl>
GpuVideoAcceleratorFactoriesImpl::Create(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<viz::ContextProviderCommandBuffer>& context_provider,
    std::unique_ptr<CodecFactory> codec_factory,
    bool enable_video_gpu_memory_buffers,
    bool enable_media_stream_gpu_memory_buffers,
    bool enable_video_decode_accelerator,
    bool enable_video_encode_accelerator) {
  return base::WrapUnique(new GpuVideoAcceleratorFactoriesImpl(
      std::move(gpu_channel_host), main_thread_task_runner, task_runner,
      std::move(context_provider), std::move(codec_factory),
      RenderThreadImpl::current()->GetGpuMemoryBufferManager(),
      enable_video_gpu_memory_buffers, enable_media_stream_gpu_memory_buffers,
      enable_video_decode_accelerator, enable_video_encode_accelerator));
}

// static
std::unique_ptr<GpuVideoAcceleratorFactoriesImpl>
GpuVideoAcceleratorFactoriesImpl::CreateForTesting(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<viz::ContextProviderCommandBuffer>& context_provider,
    std::unique_ptr<CodecFactory> codec_factory,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool enable_video_gpu_memory_buffers,
    bool enable_media_stream_gpu_memory_buffers,
    bool enable_video_decode_accelerator,
    bool enable_video_encode_accelerator) {
  return base::WrapUnique(new GpuVideoAcceleratorFactoriesImpl(
      std::move(gpu_channel_host), main_thread_task_runner, task_runner,
      std::move(context_provider), std::move(codec_factory),
      std::move(gpu_memory_buffer_manager), enable_video_gpu_memory_buffers,
      enable_media_stream_gpu_memory_buffers, enable_video_decode_accelerator,
      enable_video_encode_accelerator));
}

GpuVideoAcceleratorFactoriesImpl::GpuVideoAcceleratorFactoriesImpl(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<viz::ContextProviderCommandBuffer>& context_provider,
    std::unique_ptr<CodecFactory> codec_factory,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool enable_video_gpu_memory_buffers,
    bool enable_media_stream_gpu_memory_buffers,
    bool enable_video_decode_accelerator,
    bool enable_video_encode_accelerator)
    : main_thread_task_runner_(main_thread_task_runner),
      task_runner_(task_runner),
      gpu_channel_host_(std::move(gpu_channel_host)),
      codec_factory_(std::move(codec_factory)),
      context_provider_(context_provider),
      enable_video_gpu_memory_buffers_(enable_video_gpu_memory_buffers),
      enable_media_stream_gpu_memory_buffers_(
          enable_media_stream_gpu_memory_buffers),
      video_decode_accelerator_enabled_(enable_video_decode_accelerator),
      video_encode_accelerator_enabled_(enable_video_encode_accelerator),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager) {
  DCHECK(main_thread_task_runner_);
  DCHECK(gpu_channel_host_);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVideoAcceleratorFactoriesImpl::BindOnTaskRunner,
                     base::Unretained(this)));
}

GpuVideoAcceleratorFactoriesImpl::~GpuVideoAcceleratorFactoriesImpl() {}

void GpuVideoAcceleratorFactoriesImpl::BindOnTaskRunner() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_provider_);

  if (context_provider_->BindToCurrentSequence() !=
      gpu::ContextResult::kSuccess) {
    OnContextLost();
    return;
  }

  context_provider_->AddObserver(this);

  // Request the channel token.
  context_provider_->GetCommandBufferProxy()->GetGpuChannel().GetChannelToken(
      base::BindOnce(&GpuVideoAcceleratorFactoriesImpl::OnChannelTokenReady,
                     base::Unretained(this)));
}

bool GpuVideoAcceleratorFactoriesImpl::IsDecoderSupportKnown() {
  return codec_factory_->IsDecoderSupportKnown();
}

void GpuVideoAcceleratorFactoriesImpl::NotifyDecoderSupportKnown(
    base::OnceClosure callback) {
  codec_factory_->NotifyDecoderSupportKnown(std::move(callback));
}

bool GpuVideoAcceleratorFactoriesImpl::IsEncoderSupportKnown() {
  return codec_factory_->IsEncoderSupportKnown();
}

void GpuVideoAcceleratorFactoriesImpl::NotifyEncoderSupportKnown(
    base::OnceClosure callback) {
  codec_factory_->NotifyEncoderSupportKnown(std::move(callback));
}

bool GpuVideoAcceleratorFactoriesImpl::CheckContextLost() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (context_provider_lost_on_media_thread_)
    return true;
  if (context_provider_->ContextGL()->GetGraphicsResetStatusKHR() !=
      GL_NO_ERROR) {
    OnContextLost();
    return true;
  }
  return false;
}

void GpuVideoAcceleratorFactoriesImpl::DestroyContext() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_provider_lost_on_media_thread_);

  if (!context_provider_)
    return;

  context_provider_->RemoveObserver(this);
  context_provider_ = nullptr;
}

bool GpuVideoAcceleratorFactoriesImpl::IsGpuVideoDecodeAcceleratorEnabled() {
  return video_decode_accelerator_enabled_;
}
bool GpuVideoAcceleratorFactoriesImpl::IsGpuVideoEncodeAcceleratorEnabled() {
  return video_encode_accelerator_enabled_;
}

void GpuVideoAcceleratorFactoriesImpl::GetChannelToken(
    gpu::mojom::GpuChannel::GetChannelTokenCallback cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (CheckContextLost()) {
    std::move(cb).Run(base::UnguessableToken());
    return;
  }

  if (!channel_token_.is_empty()) {
    // Use cached token.
    std::move(cb).Run(channel_token_);
    return;
  }

  // Retrieve a channel token if needed.
  channel_token_callbacks_.AddUnsafe(std::move(cb));
}

void GpuVideoAcceleratorFactoriesImpl::OnChannelTokenReady(
    const base::UnguessableToken& token) {
  channel_token_ = token;
  channel_token_callbacks_.Notify(channel_token_);
  DCHECK(channel_token_callbacks_.empty());
  codec_factory_->OnChannelTokenReady(
      token, context_provider_->GetCommandBufferProxy()->route_id());
}

int32_t GpuVideoAcceleratorFactoriesImpl::GetCommandBufferRouteId() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (CheckContextLost())
    return 0;
  return context_provider_->GetCommandBufferProxy()->route_id();
}

media::GpuVideoAcceleratorFactories::Supported
GpuVideoAcceleratorFactoriesImpl::IsDecoderConfigSupported(
    const media::VideoDecoderConfig& config) {
  // There is no support for alpha channel hardware decoding yet.
  // HEVC is the codec that only has platform hardware decoder support, and
  // macOS currently support HEVC with alpha, so don't block HEVC here.
  if (config.alpha_mode() == media::VideoDecoderConfig::AlphaMode::kHasAlpha &&
      config.codec() != media::VideoCodec::kHEVC) {
    DVLOG(1) << "Alpha transparency formats are not supported.";
    return Supported::kFalse;
  }

  auto supported_decoder_configs =
      codec_factory_->GetSupportedVideoDecoderConfigs();
  if (!supported_decoder_configs) {
    return Supported::kUnknown;
  }

  // Iterate over the supported configs.
  for (const auto& supported : *supported_decoder_configs) {
    if (supported.Matches(config))
      return Supported::kTrue;
  }
  return Supported::kFalse;
}

media::VideoDecoderType GpuVideoAcceleratorFactoriesImpl::GetDecoderType() {
  return codec_factory_->GetVideoDecoderType();
}

std::unique_ptr<media::VideoDecoder>
GpuVideoAcceleratorFactoriesImpl::CreateVideoDecoder(
    media::MediaLog* media_log,
    media::RequestOverlayInfoCB request_overlay_info_cb) {
  DCHECK(video_decode_accelerator_enabled_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (CheckContextLost())
    return nullptr;

  return codec_factory_->CreateVideoDecoder(
      this, media_log, request_overlay_info_cb, rendering_color_space_);
}

std::unique_ptr<media::VideoEncodeAccelerator>
GpuVideoAcceleratorFactoriesImpl::CreateVideoEncodeAccelerator() {
  DCHECK(video_encode_accelerator_enabled_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (CheckContextLost())
    return nullptr;

  return codec_factory_->CreateVideoEncodeAccelerator();
}

std::unique_ptr<gfx::GpuMemoryBuffer>
GpuVideoAcceleratorFactoriesImpl::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
      size, format, usage, gpu::kNullSurfaceHandle, nullptr);
}
bool GpuVideoAcceleratorFactoriesImpl::ShouldUseGpuMemoryBuffersForVideoFrames(
    bool for_media_stream) const {
  return for_media_stream ? enable_media_stream_gpu_memory_buffers_
                          : enable_video_gpu_memory_buffers_;
}

media::GpuVideoAcceleratorFactories::OutputFormat
GpuVideoAcceleratorFactoriesImpl::VideoFrameOutputFormat(
    media::VideoPixelFormat pixel_format) {
  auto format = VideoFrameOutputFormatImpl(pixel_format);
  UMA_HISTOGRAM_ENUMERATION("Media.GPU.OutputFormat", format);
  return format;
}

media::GpuVideoAcceleratorFactories::OutputFormat
GpuVideoAcceleratorFactoriesImpl::VideoFrameOutputFormatImpl(
    media::VideoPixelFormat pixel_format) {
  using OutputFormat = media::GpuVideoAcceleratorFactories::OutputFormat;

  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (CheckContextLost()) {
    return media::GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(IS_OZONE)
  // TODO(sugoi): This configuration is currently used only for testing ChromeOS
  // on Linux and doesn't support hardware acceleration. OSMesa did not support
  // any hardware acceleration here, so this was never an issue, but SwiftShader
  // revealed this issue. See https://crbug.com/859946
  if (gpu_channel_host_->gpu_info().gl_renderer.find("SwiftShader") !=
      std::string::npos) {
    return OutputFormat::UNDEFINED;
  }
#endif
  auto capabilities = context_provider_->ContextCapabilities();
  const auto& shared_image_capabilities =
      context_provider_->SharedImageInterface()->GetCapabilities();
  const size_t bit_depth = media::BitDepth(pixel_format);
  if (bit_depth > 8) {
    if (capabilities.image_ycbcr_p010 && bit_depth == 10) {
      return OutputFormat::P010;
    }

#if !BUILDFLAG(IS_MAC)
    // If high bit depth rendering is enabled, bail here, otherwise try and use
    // XR30 storage, and if not and we support RG textures, use those, albeit at
    // a reduced bit depth of 8 bits per component.
    // TODO(mcasas): continue working on this, avoiding dropping information as
    // long as the hardware may support it https://crbug.com/798485.
    if (rendering_color_space_.IsHDR()) {
      return OutputFormat::UNDEFINED;
    }
#endif  // !BUILDFLAG(IS_MAC)

#if !BUILDFLAG(IS_WIN)
    // TODO(mcasas): enable Win https://crbug.com/803451.
    // TODO(mcasas): remove the |bit_depth| check when libyuv supports more than
    // just x010ToAR30 conversions, https://crbug.com/libyuv/751.
    if (bit_depth == 10) {
      if (capabilities.image_ar30) {
        return OutputFormat::XR30;
      } else if (capabilities.image_ab30) {
        return OutputFormat::XB30;
      }
    }
#endif  // !BUILDFLAG(IS_WIN)
    if (capabilities.texture_rg) {
#if BUILDFLAG(IS_WIN)
      // Use NV12 for Windows platform which has the overlay support.
      if (base::FeatureList::IsEnabled(kUseNV12OutputFormat)) {
        return OutputFormat::NV12;
      }
#endif
      return OutputFormat::YV12;
    }
    return OutputFormat::UNDEFINED;
  }

#if BUILDFLAG(IS_FUCHSIA)
  // Hardware support for NV12 GMBs is expected to be present on all supported
  // Fuchsia devices.
  CHECK(capabilities.image_ycbcr_420v);
  CHECK(shared_image_capabilities.supports_native_nv12_mappable_shared_images);
  return OutputFormat::NV12;
#else

  if (capabilities.image_ycbcr_420v &&
      shared_image_capabilities.supports_native_nv12_mappable_shared_images) {
    return OutputFormat::NV12;
  }

  // For ChromeOS, if above hardware support for NV12 is not present then
  // fallback to pixel upload.
#if !BUILDFLAG(IS_CHROMEOS)
  if (capabilities.texture_rg) {
    // Use NV12 for Mac, Windows, Linux and CastOS platforms.
    return OutputFormat::NV12;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  return OutputFormat::UNDEFINED;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

gpu::SharedImageInterface*
GpuVideoAcceleratorFactoriesImpl::SharedImageInterface() {
  return CheckContextLost() ? nullptr
                            : context_provider_->SharedImageInterface();
}

gpu::GpuMemoryBufferManager*
GpuVideoAcceleratorFactoriesImpl::GpuMemoryBufferManager() {
  return gpu_memory_buffer_manager_;
}

base::UnsafeSharedMemoryRegion
GpuVideoAcceleratorFactoriesImpl::CreateSharedMemoryRegion(size_t size) {
  // If necessary, this call will make a synchronous request to a privileged
  // process to create the shared region.
  return base::UnsafeSharedMemoryRegion::Create(size);
}

scoped_refptr<base::SequencedTaskRunner>
GpuVideoAcceleratorFactoriesImpl::GetTaskRunner() {
  return task_runner_;
}

std::optional<media::VideoEncodeAccelerator::SupportedProfiles>
GpuVideoAcceleratorFactoriesImpl::GetVideoEncodeAcceleratorSupportedProfiles() {
  return codec_factory_->GetVideoEncodeAcceleratorSupportedProfiles();
}

viz::RasterContextProvider*
GpuVideoAcceleratorFactoriesImpl::GetMediaContextProvider() {
  return CheckContextLost() ? nullptr : context_provider_.get();
}

const gpu::Capabilities*
GpuVideoAcceleratorFactoriesImpl::ContextCapabilities() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return CheckContextLost() ? nullptr
                            : &(context_provider_->ContextCapabilities());
}

void GpuVideoAcceleratorFactoriesImpl::SetRenderingColorSpace(
    const gfx::ColorSpace& color_space) {
  rendering_color_space_ = color_space;
}

const gfx::ColorSpace&
GpuVideoAcceleratorFactoriesImpl::GetRenderingColorSpace() const {
  return rendering_color_space_;
}

bool GpuVideoAcceleratorFactoriesImpl::CheckContextProviderLostOnMainThread() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return context_provider_lost_;
}

void GpuVideoAcceleratorFactoriesImpl::OnContextLost() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("media", "GpuVideoAcceleratorFactoriesImpl::OnContextLost");

  // Don't delete the |context_provider_| here, we could be in the middle of
  // it notifying about the loss, and we'd be destroying it while it's on
  // the stack.
  context_provider_lost_on_media_thread_ = true;
  // Inform the main thread of the loss as well, so that this class can be
  // replaced.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuVideoAcceleratorFactoriesImpl::SetContextProviderLostOnMainThread,
          base::Unretained(this)));
}

void GpuVideoAcceleratorFactoriesImpl::SetContextProviderLostOnMainThread() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  context_provider_lost_ = true;
}

}  // namespace content
