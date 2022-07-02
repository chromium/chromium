// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/stable_video_decoder_factory_service.h"

#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/chromeos/video_frame_converter.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/mojo_video_decoder_service.h"
#include "media/mojo/services/stable_video_decoder_service.h"
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
    // TODO(b/195769334): we should pass a meaningful
    // gpu::GpuDriverBugWorkarounds so that we can restrict the supported
    // configurations using that facility.
    absl::optional<std::vector<SupportedVideoDecoderConfig>> configs =
        VideoDecoderPipeline::GetSupportedConfigs(
            gpu::GpuDriverBugWorkarounds());
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
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder)
      final {
    // TODO(b/195769334): some platforms do not support the
    // VideoDecoderPipeline so we need to handle those (and the rest of the
    // methods of MojoMediaClientImpl are affected as well).

    // For out-of-process video decoding, |command_buffer_id| is not used and
    // should not be supplied.
    DCHECK(!command_buffer_id);

    DCHECK(!oop_video_decoder);

    std::unique_ptr<MediaLog> log =
        media_log ? media_log->Clone()
                  : std::make_unique<media::NullMediaLog>();
    return VideoDecoderPipeline::Create(
        /*client_task_runner=*/std::move(task_runner),
        std::make_unique<PlatformVideoFramePool>(),
        std::make_unique<media::VideoFrameConverter>(), std::move(log),
        /*oop_video_decoder=*/{});
  }
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
  video_decoders_.Add(
      std::make_unique<StableVideoDecoderService>(std::move(dst_video_decoder)),
      std::move(receiver));
}

}  // namespace media
