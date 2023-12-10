// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_encoder.h"
#include "media/base/media_switches.h"
#include "media/gpu/chromeos/mailbox_video_frame_converter.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/ipc/service/vda_video_decoder.h"

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

  if (IsOutOfProcessVideoDecodingEnabled()) {
    return VideoDecoderType::kOutOfProcess;
  }

  if (gpu_preferences.enable_chromeos_direct_video_decoder) {
#if BUILDFLAG(USE_VAAPI)
    return VideoDecoderType::kVaapi;
#elif BUILDFLAG(USE_V4L2_CODEC)
    return VideoDecoderType::kV4L2;
#endif
  }
  return VideoDecoderType::kVda;
}

}  // namespace

std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
    VideoDecoderTraits& traits) {
  const auto decoder_type =
      GetActualPlatformDecoderImplementation(traits.gpu_preferences);
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
          traits.gpu_task_runner, traits.get_command_buffer_stub_cb);
      return VideoDecoderPipeline::Create(
          *traits.gpu_workarounds, traits.task_runner, std::move(frame_pool),
          std::move(frame_converter),
          GetPreferredRenderableFourccs(traits.gpu_preferences),
          traits.media_log->Clone(), std::move(traits.oop_video_decoder),
          /*in_video_decoder_process=*/false);
    }
    case VideoDecoderType::kVaapi:
    case VideoDecoderType::kV4L2: {
      auto frame_pool = std::make_unique<PlatformVideoFramePool>();
      auto frame_converter = MailboxVideoFrameConverter::Create(
          traits.gpu_task_runner, traits.get_command_buffer_stub_cb);
      return VideoDecoderPipeline::Create(
          *traits.gpu_workarounds, traits.task_runner, std::move(frame_pool),
          std::move(frame_converter),
          GetPreferredRenderableFourccs(traits.gpu_preferences),
          traits.media_log->Clone(), /*oop_video_decoder=*/{},
          /*in_video_decoder_process=*/false);
    }
    case VideoDecoderType::kVda: {
      return VdaVideoDecoder::Create(
          traits.task_runner, traits.gpu_task_runner, traits.media_log->Clone(),
          *traits.target_color_space, traits.gpu_preferences,
          *traits.gpu_workarounds, traits.get_command_buffer_stub_cb,
          VideoDecodeAccelerator::Config::OutputMode::kAllocate);
    }
    default: {
      return nullptr;
    }
  }
}

void NotifyPlatformDecoderSupport(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
    base::OnceCallback<
        void(mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb) {
  switch (GetActualPlatformDecoderImplementation(gpu_preferences)) {
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

absl::optional<SupportedVideoDecoderConfigs>
GetPlatformSupportedVideoDecoderConfigs(
    base::WeakPtr<MediaGpuChannelManager> manager,
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    base::OnceCallback<SupportedVideoDecoderConfigs()> get_vda_configs) {
  VideoDecoderType decoder_implementation =
      GetActualPlatformDecoderImplementation(gpu_preferences);
  switch (decoder_implementation) {
    case VideoDecoderType::kVda:
      return std::move(get_vda_configs).Run();
    case VideoDecoderType::kOutOfProcess:
    case VideoDecoderType::kVaapi:
    case VideoDecoderType::kV4L2:
      return VideoDecoderPipeline::GetSupportedConfigs(decoder_implementation,
                                                       gpu_workarounds);
    default:
      return absl::nullopt;
  }
}

VideoDecoderType GetPlatformDecoderImplementationType(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info) {
  // Determine the preferred decoder based purely on compile-time and run-time
  // flags. This is not intended to determine whether the selected decoder can
  // be successfully initialized or used to decode.
  return GetActualPlatformDecoderImplementation(gpu_preferences);
}

std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log) {
  return nullptr;
}

std::unique_ptr<AudioEncoder> CreatePlatformAudioEncoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return nullptr;
}

std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return std::make_unique<chromeos::ChromeOsCdmFactory>(frame_interfaces);
}

}  // namespace media
