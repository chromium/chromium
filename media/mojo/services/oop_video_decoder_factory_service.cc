// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/oop_video_decoder_factory_service.h"

#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/viz/common/switches.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/dmabuf_video_frame_converter.h"
#include "media/gpu/chromeos/frame_registry.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/mojo_video_decoder_service.h"
#include "media/mojo/services/oop_video_decoder_service.h"
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
  explicit MojoMediaClientImpl(const gpu::GpuFeatureInfo& gpu_feature_info)
      : gpu_driver_bug_workarounds_(
            gpu_feature_info.enabled_gpu_driver_bug_workarounds) {}
  MojoMediaClientImpl(const MojoMediaClientImpl&) = delete;
  MojoMediaClientImpl& operator=(const MojoMediaClientImpl&) = delete;
  ~MojoMediaClientImpl() override = default;

  // MojoMediaClient implementation.
  std::vector<SupportedVideoDecoderConfig> GetSupportedVideoDecoderConfigs()
      final {
    std::optional<std::vector<SupportedVideoDecoderConfig>> configs;
    switch (GetDecoderImplementationType()) {
      case VideoDecoderType::kVaapi:
      case VideoDecoderType::kV4L2:
        configs = VideoDecoderPipeline::GetSupportedConfigs(
            GetDecoderImplementationType(), gpu_driver_bug_workarounds_);
        break;
      default:
        NOTREACHED();
    }
    return configs.value_or(std::vector<SupportedVideoDecoderConfig>{});
  }
  VideoDecoderType GetDecoderImplementationType() final {
    // TODO(b/195769334): how can we keep this in sync with
    // VideoDecoderPipeline::GetDecoderType()?
#if BUILDFLAG(USE_VAAPI)
    return VideoDecoderType::kVaapi;
#elif BUILDFLAG(USE_V4L2_CODEC)
    return VideoDecoderType::kV4L2;
#else
#error OOPVideoDecoderFactoryService should only be built on platforms that
#error support video decode acceleration through either VA-API or V4L2.
#endif
  }
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      mojo::PendingRemote<mojom::VideoDecoder> oop_video_decoder) final {
    // For out-of-process video decoding, |command_buffer_id| is not used and
    // should not be supplied.
    DCHECK(!command_buffer_id);

    DCHECK(!oop_video_decoder);

    std::unique_ptr<MediaLog> log =
        media_log ? media_log->Clone()
                  : std::make_unique<media::NullMediaLog>();

    CHECK_NE(GetDecoderImplementationType(), VideoDecoderType::kVda);
    return VideoDecoderPipeline::Create(
        gpu_driver_bug_workarounds_,
        /*client_task_runner=*/std::move(task_runner),
        std::make_unique<PlatformVideoFramePool>(),
        DmabufVideoFrameConverter::Create(),
        VideoDecoderPipeline::DefaultPreferredRenderableFourccs(),
        std::move(log),
        /*oop_video_decoder=*/{},
        /*in_video_decoder_process=*/true);
  }

 private:
  const gpu::GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;
};

}  // namespace

OOPVideoDecoderFactoryService::OOPVideoDecoderFactoryService(
    const gpu::GpuFeatureInfo& gpu_feature_info)
    : receiver_(this),
      mojo_media_client_(
          std::make_unique<MojoMediaClientImpl>(gpu_feature_info)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo_media_client_->Initialize();
}

OOPVideoDecoderFactoryService::~OOPVideoDecoderFactoryService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OOPVideoDecoderFactoryService::BindReceiver(
    mojo::PendingReceiver<mojom::InterfaceFactory> receiver,
    base::OnceClosure disconnect_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The browser process should guarantee that BindReceiver() is only called
  // once.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(std::move(disconnect_cb));
}

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
void OOPVideoDecoderFactoryService::CreateVideoDecoderWithTracker(
    mojo::PendingReceiver<mojom::VideoDecoder> receiver,
    mojo::PendingRemote<mojom::VideoDecoderTracker> tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<mojom::VideoDecoder> dst_video_decoder;
  if (video_decoder_creation_cb_for_testing_) {
    dst_video_decoder = video_decoder_creation_cb_for_testing_.Run(
        mojo_media_client_.get(), &cdm_service_context_);
  } else {
    dst_video_decoder = std::make_unique<MojoVideoDecoderService>(
        mojo_media_client_.get(), &cdm_service_context_,
        mojo::PendingRemote<mojom::VideoDecoder>());
  }
  video_decoders_.Add(std::make_unique<OOPVideoDecoderService>(
                          std::move(tracker), std::move(dst_video_decoder),
                          &cdm_service_context_),
                      std::move(receiver));
}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

// The client of the OOPVideoDecoderFactoryService is the browser process which
// is up the trust gradient. The browser process should never use this service
// for anything other than creating video decoders. Therefore, it's appropriate
// to crash in the following methods via NOTREACHED().
void OOPVideoDecoderFactoryService::CreateAudioDecoder(
    mojo::PendingReceiver<mojom::AudioDecoder> receiver) {
  NOTREACHED();
}

void OOPVideoDecoderFactoryService::CreateVideoDecoder(
    mojo::PendingReceiver<mojom::VideoDecoder> receiver,
    mojo::PendingRemote<media::mojom::VideoDecoder> dst_video_decoder) {
  NOTREACHED();
}

void OOPVideoDecoderFactoryService::CreateAudioEncoder(
    mojo::PendingReceiver<mojom::AudioEncoder> receiver) {
  NOTREACHED();
}

void OOPVideoDecoderFactoryService::CreateDefaultRenderer(
    const std::string& audio_device_id,
    mojo::PendingReceiver<mojom::Renderer> receiver) {
  NOTREACHED();
}

void OOPVideoDecoderFactoryService::CreateCdm(const CdmConfig& cdm_config,
                                              CreateCdmCallback callback) {
  NOTREACHED();
}

}  // namespace media
