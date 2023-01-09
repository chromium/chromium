// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/stable_video_decoder_factory_service.h"

#include "base/command_line.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/viz/common/switches.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/chromeos/video_frame_converter.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/ipc/service/vda_video_decoder.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/mojo_video_decoder_service.h"
#include "media/mojo/services/stable_video_decoder_service.h"
#include "media/video/video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace media {

namespace {

// This is a lighter alternative to using a GpuMojoMediaClient. While we could
// use a GpuMojoMediaClient, that would be abusing the abstraction a bit since
// that class is too semantically coupled with the GPU process through things
// like its |gpu_task_runner_| and |media_gpu_channel_manager_| members.
class MojoMediaClientImpl : public MojoMediaClient {
 public:
  MojoMediaClientImpl() = default;
  MojoMediaClientImpl(const MojoMediaClientImpl&) = delete;
  MojoMediaClientImpl& operator=(const MojoMediaClientImpl&) = delete;
  ~MojoMediaClientImpl() override = default;

  // MojoMediaClient implementation.
  std::vector<SupportedVideoDecoderConfig> GetSupportedVideoDecoderConfigs()
      final {
    // TODO(b/195769334): we should pass meaningful gpu::GpuPreferences and
    // gpu::GpuDriverBugWorkarounds so that we can restrict the supported
    // configurations using that facility.
    absl::optional<std::vector<SupportedVideoDecoderConfig>> configs;
    switch (GetDecoderImplementationType()) {
      case VideoDecoderType::kVaapi:
      case VideoDecoderType::kV4L2:
        configs = VideoDecoderPipeline::GetSupportedConfigs(
            gpu::GpuDriverBugWorkarounds());
        break;
      case VideoDecoderType::kVda: {
        VideoDecodeAccelerator::Capabilities capabilities =
            GpuVideoAcceleratorUtil::ConvertGpuToMediaDecodeCapabilities(
                GpuVideoDecodeAcceleratorFactory::GetDecoderCapabilities(
                    gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds()));
        configs = ConvertFromSupportedProfiles(
            capabilities.supported_profiles,
            capabilities.flags & VideoDecodeAccelerator::Capabilities::
                                     SUPPORTS_ENCRYPTED_STREAMS);
        break;
      }
      default:
        NOTREACHED();
    }
    return configs.value_or(std::vector<SupportedVideoDecoderConfig>{});
  }
  VideoDecoderType GetDecoderImplementationType() final {
#if BUILDFLAG(IS_CHROMEOS)
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kPlatformDisallowsChromeOSDirectVideoDecoder)) {
      return VideoDecoderType::kVda;
    }
#endif  // BUILDFLAG(IS_CHROMEOS)

    // TODO(b/195769334): how can we keep this in sync with
    // VideoDecoderPipeline::GetDecoderType()?
#if BUILDFLAG(USE_VAAPI)
    return VideoDecoderType::kVaapi;
#elif BUILDFLAG(USE_V4L2_CODEC)
    return VideoDecoderType::kV4L2;
#else
#error StableVideoDecoderFactoryService should only be built on platforms that
#error support video decode acceleration through either VA-API or V4L2.
#endif
  }
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder)
      final {
    // For out-of-process video decoding, |command_buffer_id| is not used and
    // should not be supplied.
    DCHECK(!command_buffer_id);

    DCHECK(!oop_video_decoder);

    std::unique_ptr<MediaLog> log =
        media_log ? media_log->Clone()
                  : std::make_unique<media::NullMediaLog>();

    if (GetDecoderImplementationType() == VideoDecoderType::kVda) {
      if (!gpu_task_runner_) {
        gpu_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
            {base::WithBaseSyncPrimitives(), base::MayBlock()},
            base::SingleThreadTaskRunnerThreadMode::DEDICATED);
      }
      // TODO(b/195769334): we should pass meaningful gpu::Preferences and
      // gpu::GpuDriverBugWorkarounds so that we can restrict the supported
      // configurations using that facility.
      return VdaVideoDecoder::Create(
          /*parent_task_runner=*/std::move(task_runner), gpu_task_runner_,
          std::move(log), target_color_space, gpu::GpuPreferences(),
          gpu::GpuDriverBugWorkarounds(),
          /*get_stub_cb=*/base::NullCallback(),
          VideoDecodeAccelerator::Config::OutputMode::IMPORT);
    } else {
      return VideoDecoderPipeline::Create(
          // TODO(b/195769334): we should pass a meaningful
          // gpu::GpuDriverBugWorkarounds so that we can restrict the supported
          // configurations using that facility.
          gpu::GpuDriverBugWorkarounds(),
          /*client_task_runner=*/std::move(task_runner),
          std::make_unique<PlatformVideoFramePool>(),
          std::make_unique<media::VideoFrameConverter>(), std::move(log),
          /*oop_video_decoder=*/{});
    }
  }

 private:
  // A "GPU" thread. With traditional hardware video decoding that runs in the
  // GPU process, this would be the thread needed to access specific GPU
  // functionality. For out-of-process video decoding, this isn't really the
  // "GPU" thread, but we use the terminology of the VdaVideoDecoder::Create()
  // (as such this member is only used when using the VdaVideoDecoder).
  //
  // TODO(b/195769334): could we get rid of this and just use the same task
  // runner for the |parent_task_runner| and |gpu_task_runner| parameters of
  // VdaVideoDecoder::Create(). For now, we've made it a dedicated thread in
  // case the VdaVideoDecoder or any of the underlying components rely on a
  // separate GPU thread.
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
};

}  // namespace

StableVideoDecoderFactoryService::StableVideoDecoderFactoryService()
    : receiver_(this),
      mojo_media_client_(std::make_unique<MojoMediaClientImpl>()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo_media_client_->Initialize();
}

StableVideoDecoderFactoryService::~StableVideoDecoderFactoryService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StableVideoDecoderFactoryService::BindReceiver(
    mojo::PendingReceiver<stable::mojom::StableVideoDecoderFactory> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The browser process should guarantee that BindReceiver() is only called
  // once.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
}

void StableVideoDecoderFactoryService::CreateStableVideoDecoder(
    mojo::PendingReceiver<stable::mojom::StableVideoDecoder> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<mojom::VideoDecoder> dst_video_decoder;
  if (video_decoder_creation_cb_for_testing_) {
    dst_video_decoder = video_decoder_creation_cb_for_testing_.Run(
        mojo_media_client_.get(), &cdm_service_context_);
  } else {
    dst_video_decoder = std::make_unique<MojoVideoDecoderService>(
        mojo_media_client_.get(), &cdm_service_context_,
        mojo::PendingRemote<stable::mojom::StableVideoDecoder>());
  }
  video_decoders_.Add(std::make_unique<StableVideoDecoderService>(
                          std::move(dst_video_decoder), &cdm_service_context_),
                      std::move(receiver));
}

}  // namespace media
