// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/gpu/gpu_video_accelerator_factories_impl.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/content_features.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/bind_to_current_loop.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/clients/mojo_video_encode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace content {

namespace {

// This enum values match ContextProviderPhase in histograms.xml
enum ContextProviderPhase {
  CONTEXT_PROVIDER_ACQUIRED = 0,
  CONTEXT_PROVIDER_RELEASED = 1,
  CONTEXT_PROVIDER_RELEASED_MAX_VALUE = CONTEXT_PROVIDER_RELEASED,
};

void RecordContextProviderPhaseUmaEnum(const ContextProviderPhase phase) {
  UMA_HISTOGRAM_ENUMERATION("Media.GPU.HasEverLostContext", phase,
                            CONTEXT_PROVIDER_RELEASED_MAX_VALUE + 1);
}

}  // namespace

GpuVideoAcceleratorFactoriesImpl::Notifier::Notifier() = default;
GpuVideoAcceleratorFactoriesImpl::Notifier::~Notifier() = default;

void GpuVideoAcceleratorFactoriesImpl::Notifier::Register(
    base::OnceClosure callback) {
  if (is_notified_) {
    std::move(callback).Run();
    return;
  }
  callbacks_.push_back(std::move(callback));
}

void GpuVideoAcceleratorFactoriesImpl::Notifier::Notify() {
  DCHECK(!is_notified_);
  is_notified_ = true;
  for (auto& callback : callbacks_)
    std::move(callback).Run();
  callbacks_.clear();
}

// static
std::unique_ptr<GpuVideoAcceleratorFactoriesImpl>
GpuVideoAcceleratorFactoriesImpl::Create(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<viz::ContextProviderCommandBuffer>& context_provider,
    bool enable_video_gpu_memory_buffers,
    bool enable_media_stream_gpu_memory_buffers,
    bool enable_video_decode_accelerator,
    bool enable_video_encode_accelerator,
    mojo::PendingRemote<media::mojom::InterfaceFactory>
        interface_factory_remote,
    mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
        vea_provider_remote) {
  RecordContextProviderPhaseUmaEnum(
      ContextProviderPhase::CONTEXT_PROVIDER_ACQUIRED);
  return base::WrapUnique(new GpuVideoAcceleratorFactoriesImpl(
      std::move(gpu_channel_host), main_thread_task_runner, task_runner,
      context_provider, enable_video_gpu_memory_buffers,
      enable_media_stream_gpu_memory_buffers, enable_video_decode_accelerator,
      enable_video_encode_accelerator, std::move(interface_factory_remote),
      std::move(vea_provider_remote)));
}

GpuVideoAcceleratorFactoriesImpl::GpuVideoAcceleratorFactoriesImpl(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<viz::ContextProviderCommandBuffer>& context_provider,
    bool enable_video_gpu_memory_buffers,
    bool enable_media_stream_gpu_memory_buffers,
    bool enable_video_decode_accelerator,
    bool enable_video_encode_accelerator,
    mojo::PendingRemote<media::mojom::InterfaceFactory>
        interface_factory_remote,
    mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
        vea_provider_remote)
    : main_thread_task_runner_(main_thread_task_runner),
      task_runner_(task_runner),
      gpu_channel_host_(std::move(gpu_channel_host)),
      context_provider_(context_provider),
      enable_video_gpu_memory_buffers_(enable_video_gpu_memory_buffers),
      enable_media_stream_gpu_memory_buffers_(
          enable_media_stream_gpu_memory_buffers),
      video_decode_accelerator_enabled_(enable_video_decode_accelerator),
      video_encode_accelerator_enabled_(enable_video_encode_accelerator),
      gpu_memory_buffer_manager_(
          RenderThreadImpl::current()->GetGpuMemoryBufferManager()) {
  DCHECK(main_thread_task_runner_);
  DCHECK(gpu_channel_host_);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVideoAcceleratorFactoriesImpl::BindOnTaskRunner,
                     base::Unretained(this),
                     std::move(interface_factory_remote),
                     std::move(vea_provider_remote)));
}

GpuVideoAcceleratorFactoriesImpl::~GpuVideoAcceleratorFactoriesImpl() {}

void GpuVideoAcceleratorFactoriesImpl::BindOnTaskRunner(
    mojo::PendingRemote<media::mojom::InterfaceFactory>
        interface_factory_remote,
    mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
        vea_provider_remote) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_provider_);

  interface_factory_.Bind(std::move(interface_factory_remote));
  vea_provider_.Bind(std::move(vea_provider_remote));

  if (context_provider_->BindToCurrentSequence() !=
      gpu::ContextResult::kSuccess) {
    OnDecoderSupportFailed();
    OnEncoderSupportFailed();
    OnContextLost();
    return;
  }

  context_provider_->AddObserver(this);

  // Request the channel token.
  context_provider_->GetCommandBufferProxy()->GetGpuChannel().GetChannelToken(
      base::BindOnce(&GpuVideoAcceleratorFactoriesImpl::OnChannelTokenReady,
                     base::Unretained(this)));

  if (video_encode_accelerator_enabled_) {
    vea_provider_.set_disconnect_handler(base::BindOnce(
        &GpuVideoAcceleratorFactoriesImpl::OnEncoderSupportFailed,
        base::Unretained(this)));
    vea_provider_->GetVideoEncodeAcceleratorSupportedProfiles(
        base::BindOnce(&GpuVideoAcceleratorFactoriesImpl::
                           OnGetVideoEncodeAcceleratorSupportedProfiles,
                       base::Unretained(this)));
  } else {
    OnEncoderSupportFailed();
  }

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  if (video_decode_accelerator_enabled_) {
    // Note: This is a bit of a hack, since we don't specify the implementation
    // before asking for the map of supported configs.  We do this because it
    // (a) saves an ipc call, and (b) makes the return of those configs atomic.
    interface_factory_->CreateVideoDecoder(
        video_decoder_.BindNewPipeAndPassReceiver(), /*dst_video_decoder=*/{});
    video_decoder_.set_disconnect_handler(base::BindOnce(
        &GpuVideoAcceleratorFactoriesImpl::OnDecoderSupportFailed,
        base::Unretained(this)));
    video_decoder_->GetSupportedConfigs(base::BindOnce(
        &GpuVideoAcceleratorFactoriesImpl::OnSupportedDecoderConfigs,
        base::Unretained(this)));
  } else {
    OnDecoderSupportFailed();
  }
#else
  OnDecoderSupportFailed();
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
}

bool GpuVideoAcceleratorFactoriesImpl::IsDecoderSupportKnown() {
  base::AutoLock lock(supported_profiles_lock_);
  return decoder_support_notifier_.is_notified();
}

void GpuVideoAcceleratorFactoriesImpl::NotifyDecoderSupportKnown(
    base::OnceClosure callback) {
  base::AutoLock lock(supported_profiles_lock_);
  decoder_support_notifier_.Register(
      media::BindToCurrentLoop(std::move(callback)));
}

void GpuVideoAcceleratorFactoriesImpl::OnSupportedDecoderConfigs(
    const media::SupportedVideoDecoderConfigs& supported_configs,
    media::VideoDecoderType decoder_type) {
  base::AutoLock lock(supported_profiles_lock_);
  video_decoder_.reset();
  supported_decoder_configs_ = supported_configs;
  video_decoder_type_ = decoder_type;
  decoder_support_notifier_.Notify();
}

void GpuVideoAcceleratorFactoriesImpl::OnDecoderSupportFailed() {
  base::AutoLock lock(supported_profiles_lock_);
  video_decoder_.reset();
  if (decoder_support_notifier_.is_notified())
    return;
  supported_decoder_configs_ = media::SupportedVideoDecoderConfigs();
  decoder_support_notifier_.Notify();
}

bool GpuVideoAcceleratorFactoriesImpl::IsEncoderSupportKnown() {
  base::AutoLock lock(supported_profiles_lock_);
  return encoder_support_notifier_.is_notified();
}

void GpuVideoAcceleratorFactoriesImpl::NotifyEncoderSupportKnown(
    base::OnceClosure callback) {
  base::AutoLock lock(supported_profiles_lock_);
  encoder_support_notifier_.Register(
      media::BindToCurrentLoop(std::move(callback)));
}

void GpuVideoAcceleratorFactoriesImpl::
    OnGetVideoEncodeAcceleratorSupportedProfiles(
        const media::VideoEncodeAccelerator::SupportedProfiles&
            supported_profiles) {
  base::AutoLock lock(supported_profiles_lock_);
  supported_vea_profiles_ = supported_profiles;
  encoder_support_notifier_.Notify();
}

void GpuVideoAcceleratorFactoriesImpl::OnEncoderSupportFailed() {
  base::AutoLock lock(supported_profiles_lock_);
  if (encoder_support_notifier_.is_notified())
    return;
  supported_vea_profiles_ = media::VideoEncodeAccelerator::SupportedProfiles();
  encoder_support_notifier_.Notify();
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
  RecordContextProviderPhaseUmaEnum(
      ContextProviderPhase::CONTEXT_PROVIDER_RELEASED);
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

  base::AutoLock lock(supported_profiles_lock_);

  // If GetSupportedConfigs() has not completed (or was never started), report
  // that all configs are supported. Clients will find out that configs are not
  // supported when VideoDecoder::Initialize() fails.
  if (!supported_decoder_configs_)
    return Supported::kUnknown;

  // Iterate over the supported configs.
  for (const auto& supported : *supported_decoder_configs_) {
    if (supported.Matches(config))
      return Supported::kTrue;
  }
  return Supported::kFalse;
}

media::VideoDecoderType GpuVideoAcceleratorFactoriesImpl::GetDecoderType() {
  base::AutoLock lock(supported_profiles_lock_);
  return video_decoder_type_;
}

std::unique_ptr<media::VideoDecoder>
GpuVideoAcceleratorFactoriesImpl::CreateVideoDecoder(
    media::MediaLog* media_log,
    media::RequestOverlayInfoCB request_overlay_info_cb) {
  DCHECK(video_decode_accelerator_enabled_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(interface_factory_.is_bound());

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  if (CheckContextLost())
    return nullptr;

  mojo::PendingRemote<media::mojom::VideoDecoder> video_decoder;
  interface_factory_->CreateVideoDecoder(
      video_decoder.InitWithNewPipeAndPassReceiver(), /*dst_video_decoder=*/{});
  return std::make_unique<media::MojoVideoDecoder>(
      task_runner_, this, media_log, std::move(video_decoder),
      std::move(request_overlay_info_cb), rendering_color_space_);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
}

std::unique_ptr<media::VideoEncodeAccelerator>
GpuVideoAcceleratorFactoriesImpl::CreateVideoEncodeAccelerator() {
  DCHECK(video_encode_accelerator_enabled_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(vea_provider_.is_bound());
  if (CheckContextLost())
    return nullptr;

  base::AutoLock lock(supported_profiles_lock_);
  // When |supported_vea_profiles_| is empty, no hw encoder is available or
  // we have not yet gotten the supported profiles.
  if (!supported_vea_profiles_) {
    DVLOG(2) << "VEA's profiles have not yet been gotten";
  } else if (supported_vea_profiles_->empty()) {
    // There is no profile supported by VEA.
    return nullptr;
  }

  mojo::PendingRemote<media::mojom::VideoEncodeAccelerator> vea;
  vea_provider_->CreateVideoEncodeAccelerator(
      vea.InitWithNewPipeAndPassReceiver());

  if (!vea)
    return nullptr;

  return std::unique_ptr<media::VideoEncodeAccelerator>(
      new media::MojoVideoEncodeAccelerator(std::move(vea)));
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

unsigned GpuVideoAcceleratorFactoriesImpl::ImageTextureTarget(
    gfx::BufferFormat format) {
  DCHECK(context_provider_);
  return gpu::GetBufferTextureTarget(gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
                                     format,
                                     context_provider_->ContextCapabilities());
}

media::GpuVideoAcceleratorFactories::OutputFormat
GpuVideoAcceleratorFactoriesImpl::VideoFrameOutputFormat(
    media::VideoPixelFormat pixel_format) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (CheckContextLost())
    return media::GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED;
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(IS_OZONE)
  // TODO(sugoi): This configuration is currently used only for testing ChromeOS
  // on Linux and doesn't support hardware acceleration. OSMesa did not support
  // any hardware acceleration here, so this was never an issue, but SwiftShader
  // revealed this issue. See https://crbug.com/859946
  if (gpu_channel_host_->gpu_info().gl_renderer.find("SwiftShader") !=
      std::string::npos) {
    return media::GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED;
  }
#endif
  auto capabilities = context_provider_->ContextCapabilities();
  const size_t bit_depth = media::BitDepth(pixel_format);
  if (bit_depth > 8) {
    if (capabilities.image_ycbcr_p010 && bit_depth == 10)
      return media::GpuVideoAcceleratorFactories::OutputFormat::P010;

#if !BUILDFLAG(IS_MAC)
    // If high bit depth rendering is enabled, bail here, otherwise try and use
    // XR30 storage, and if not and we support RG textures, use those, albeit at
    // a reduced bit depth of 8 bits per component.
    // TODO(mcasas): continue working on this, avoiding dropping information as
    // long as the hardware may support it https://crbug.com/798485.
    if (rendering_color_space_.IsHDR())
      return media::GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED;
#endif

#if !BUILDFLAG(IS_WIN)
    // TODO(mcasas): enable Win https://crbug.com/803451.
    // TODO(mcasas): remove the |bit_depth| check when libyuv supports more than
    // just x010ToAR30 conversions, https://crbug.com/libyuv/751.
    if (bit_depth == 10) {
      if (capabilities.image_ar30)
        return media::GpuVideoAcceleratorFactories::OutputFormat::XR30;
      else if (capabilities.image_ab30)
        return media::GpuVideoAcceleratorFactories::OutputFormat::XB30;
    }
#endif
    if (capabilities.texture_rg)
      return media::GpuVideoAcceleratorFactories::OutputFormat::I420;
    return media::GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED;
  }

  if (pixel_format == media::PIXEL_FORMAT_I420A) {
#if SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
    return media::GpuVideoAcceleratorFactories::OutputFormat::BGRA;
#elif SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
    return media::GpuVideoAcceleratorFactories::OutputFormat::RGBA;
#endif
  }

  if (capabilities.image_ycbcr_420v &&
      !capabilities.image_ycbcr_420v_disabled_for_video_frames) {
    return media::GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB;
  }
  if (capabilities.texture_rg)
    return media::GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB;
  return media::GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED;
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

absl::optional<media::VideoEncodeAccelerator::SupportedProfiles>
GpuVideoAcceleratorFactoriesImpl::GetVideoEncodeAcceleratorSupportedProfiles() {
  base::AutoLock lock(supported_profiles_lock_);
  return supported_vea_profiles_;
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
