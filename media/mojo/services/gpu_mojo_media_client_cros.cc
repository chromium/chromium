// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_encoder.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/gpu/chromeos/mailbox_video_frame_converter.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"

namespace media {

namespace {

std::vector<Fourcc> GetPreferredRenderableFourccs(
    const gpu::GpuPreferences& gpu_preferences) {
  return VideoDecoderPipeline::DefaultPreferredRenderableFourccs();
}

VideoDecoderType GetActualPlatformDecoderImplementation(
    const gpu::GpuPreferences& gpu_preferences) {
  // TODO(b/195769334): eventually, we may turn off USE_VAAPI and USE_V4L2_CODEC
  // on LaCrOS if we delegate all video acceleration to ash-chrome. In those
  // cases, GetActualPlatformDecoderImplementation() won't be able to determine
  // the video API in LaCrOS.
  if (gpu_preferences.disable_accelerated_video_decode) {
    return VideoDecoderType::kUnknown;
  }

  switch (media::GetOutOfProcessVideoDecodingMode()) {
    case media::OOPVDMode::kEnabledWithGpuProcessAsProxy:
      return VideoDecoderType::kOutOfProcess;
    case media::OOPVDMode::kEnabledWithoutGpuProcessAsProxy:
      // The browser process ensures that this path is never reached for this
      // OOP-VD mode.
      NOTREACHED();
    case media::OOPVDMode::kDisabled:
      break;
  }

#if BUILDFLAG(USE_VAAPI)
  return VideoDecoderType::kVaapi;
#elif BUILDFLAG(USE_V4L2_CODEC)
  return VideoDecoderType::kV4L2;
#endif
  NOTREACHED();
}

}  // namespace

class GpuMojoMediaClientCrOS final : public GpuMojoMediaClient {
 public:
  GpuMojoMediaClientCrOS(GpuMojoMediaClientTraits& traits)
      : GpuMojoMediaClient(traits) {}
  ~GpuMojoMediaClientCrOS() final = default;

 protected:
  std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
      VideoDecoderTraits& traits) final {
    const auto decoder_type =
        GetActualPlatformDecoderImplementation(gpu_preferences_);
    // The browser process guarantees this CHECK.
    CHECK_EQ(!!traits.oop_video_decoder,
             (decoder_type == VideoDecoderType::kOutOfProcess));

    switch (decoder_type) {
      case VideoDecoderType::kOutOfProcess: {
        // TODO(b/195769334): for out-of-process video decoding, we don't need a
        // |frame_pool| because the buffers will be allocated and managed
        // out-of-process.
        auto frame_pool = std::make_unique<PlatformVideoFramePool>();

        auto frame_converter = MailboxVideoFrameConverter::Create(
            gpu_task_runner_, traits.get_command_buffer_stub_cb);
        return VideoDecoderPipeline::Create(
            gpu_workarounds_, traits.task_runner, std::move(frame_pool),
            std::move(frame_converter),
            GetPreferredRenderableFourccs(gpu_preferences_),
            traits.media_log->Clone(), std::move(traits.oop_video_decoder),
            /*in_video_decoder_process=*/false);
      }
      case VideoDecoderType::kVaapi:
      case VideoDecoderType::kV4L2: {
        auto frame_pool = std::make_unique<PlatformVideoFramePool>();
        auto frame_converter = MailboxVideoFrameConverter::Create(
            gpu_task_runner_, traits.get_command_buffer_stub_cb);
        return VideoDecoderPipeline::Create(
            gpu_workarounds_, traits.task_runner, std::move(frame_pool),
            std::move(frame_converter),
            GetPreferredRenderableFourccs(gpu_preferences_),
            traits.media_log->Clone(), /*oop_video_decoder=*/{},
            /*in_video_decoder_process=*/false);
      }
      case VideoDecoderType::kVda: {
        NOTREACHED();
      }
      default: {
        return nullptr;
      }
    }
  }

  void NotifyPlatformDecoderSupport(
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
      base::OnceCallback<void(
          mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb) final {
    switch (GetActualPlatformDecoderImplementation(gpu_preferences_)) {
      case VideoDecoderType::kOutOfProcess:
      case VideoDecoderType::kVaapi:
      case VideoDecoderType::kV4L2:
        VideoDecoderPipeline::NotifySupportKnown(std::move(oop_video_decoder),
                                                 std::move(cb));
        break;
      default:
        std::move(cb).Run(std::move(oop_video_decoder));
    }
  }

  std::optional<SupportedVideoDecoderConfigs>
  GetPlatformSupportedVideoDecoderConfigs() final {
    VideoDecoderType decoder_implementation =
        GetActualPlatformDecoderImplementation(gpu_preferences_);
    switch (decoder_implementation) {
      case VideoDecoderType::kVda:
        NOTREACHED();
      case VideoDecoderType::kOutOfProcess:
      case VideoDecoderType::kVaapi:
      case VideoDecoderType::kV4L2:
        return VideoDecoderPipeline::GetSupportedConfigs(decoder_implementation,
                                                         gpu_workarounds_);
      default:
        return std::nullopt;
    }
  }

  VideoDecoderType GetPlatformDecoderImplementationType() final {
    // Determine the preferred decoder based purely on compile-time and run-time
    // flags. This is not intended to determine whether the selected decoder can
    // be successfully initialized or used to decode.
    return GetActualPlatformDecoderImplementation(gpu_preferences_);
  }

  std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces) final {
    return std::make_unique<chromeos::ChromeOsCdmFactory>(frame_interfaces);
  }
};

std::unique_ptr<GpuMojoMediaClient> CreateGpuMediaService(
    GpuMojoMediaClientTraits& traits) {
  return std::make_unique<GpuMojoMediaClientCrOS>(traits);
}

}  // namespace media
