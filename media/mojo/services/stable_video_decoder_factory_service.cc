// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/stable_video_decoder_factory_service.h"

#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/viz/common/switches.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/mailbox_frame_registry.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/registered_mailbox_frame_converter.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
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
  MojoMediaClientImpl(
      const gpu::GpuFeatureInfo& gpu_feature_info,
      scoped_refptr<MailboxFrameRegistry> mailbox_frame_registry)
      : gpu_driver_bug_workarounds_(
            gpu_feature_info.enabled_gpu_driver_bug_workarounds),
        mailbox_frame_registry_(std::move(mailbox_frame_registry)) {}
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
        NOTREACHED_IN_MIGRATION();
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
#error StableVideoDecoderFactoryService should only be built on platforms that
#error support video decode acceleration through either VA-API or V4L2.
#endif
  }
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
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

    CHECK_NE(GetDecoderImplementationType(), VideoDecoderType::kVda);
    return VideoDecoderPipeline::Create(
        gpu_driver_bug_workarounds_,
        /*client_task_runner=*/std::move(task_runner),
        std::make_unique<PlatformVideoFramePool>(),
        RegisteredMailboxFrameConverter::Create(mailbox_frame_registry_),
        VideoDecoderPipeline::DefaultPreferredRenderableFourccs(),
        std::move(log),
        /*oop_video_decoder=*/{},
        /*in_video_decoder_process=*/true);
  }

 private:
  const gpu::GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;
  const scoped_refptr<MailboxFrameRegistry> mailbox_frame_registry_;
};

}  // namespace

StableVideoDecoderFactoryService::StableVideoDecoderFactoryService(
    const gpu::GpuFeatureInfo& gpu_feature_info)
    : receiver_(this),
      mailbox_frame_registry_(base::MakeRefCounted<MailboxFrameRegistry>()),
      mojo_media_client_(
          std::make_unique<MojoMediaClientImpl>(gpu_feature_info,
                                                mailbox_frame_registry_)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo_media_client_->Initialize();
}

StableVideoDecoderFactoryService::~StableVideoDecoderFactoryService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StableVideoDecoderFactoryService::BindReceiver(
    mojo::PendingReceiver<stable::mojom::StableVideoDecoderFactory> receiver,
    base::OnceClosure disconnect_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The browser process should guarantee that BindReceiver() is only called
  // once.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(std::move(disconnect_cb));
}

void StableVideoDecoderFactoryService::CreateStableVideoDecoder(
    mojo::PendingReceiver<stable::mojom::StableVideoDecoder> receiver,
    mojo::PendingRemote<stable::mojom::StableVideoDecoderTracker> tracker) {
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
                          std::move(tracker), std::move(dst_video_decoder),
                          &cdm_service_context_, mailbox_frame_registry_),
                      std::move(receiver));
}

}  // namespace media
