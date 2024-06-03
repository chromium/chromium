// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_client.h"

#include "base/command_line.h"
#include "base/time/default_tick_clock.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "media/base/video_color_space.h"
#include "ui/display/display_switches.h"

namespace {

#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
void UpdateVideoProfilesInternal(
    const media::SupportedVideoDecoderConfigs& supported_configs) {
  base::flat_set<media::VideoCodecProfile> media_profiles;
  for (const auto& config : supported_configs) {
    for (int profile = config.profile_min; profile <= config.profile_max;
         profile++) {
      media_profiles.insert(static_cast<media::VideoCodecProfile>(profile));
    }
  }
  media::UpdateDefaultSupportedVideoProfiles(media_profiles);
}
#endif

}  // namespace

namespace content {

void RenderMediaClient::Initialize() {
  static RenderMediaClient* client = new RenderMediaClient();
  media::SetMediaClient(client);
}

RenderMediaClient::RenderMediaClient()
    : main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
  // We'll first try to query the supported video decoder configurations
  // asynchronously. If IsSupportedVideoType() is called before we get a
  // response, that method will block if its not on the main thread or fall
  // back to querying the video decoder configurations synchronously otherwise.
  RenderThreadImpl::current()->BindHostReceiver(
      interface_factory_for_supported_profiles_.BindNewPipeAndPassReceiver());
  interface_factory_for_supported_profiles_.set_disconnect_handler(
      base::BindOnce(&RenderMediaClient::OnGetSupportedVideoDecoderConfigs,
                     // base::Unretained(this) is safe because the MediaClient
                     // is never destructed.
                     base::Unretained(this),
                     media::SupportedVideoDecoderConfigs(),
                     media::VideoDecoderType::kUnknown));

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  switch (media::GetOutOfProcessVideoDecodingMode()) {
    case media::OOPVDMode::kEnabledWithoutGpuProcessAsProxy: {
      mojo::SharedRemote<media::stable::mojom::StableVideoDecoder>
          stable_video_decoder_remote;
      interface_factory_for_supported_profiles_->CreateStableVideoDecoder(
          stable_video_decoder_remote.BindNewPipeAndPassReceiver());
      stable_video_decoder_remote.set_disconnect_handler(
          base::BindOnce(&RenderMediaClient::OnGetSupportedVideoDecoderConfigs,
                         // base::Unretained(this) is safe because the
                         // MediaClient is never destructed.
                         base::Unretained(this),
                         media::SupportedVideoDecoderConfigs(),
                         media::VideoDecoderType::kUnknown),
          main_task_runner_);
      stable_video_decoder_remote->GetSupportedConfigs(
          base::BindOnce(&RenderMediaClient::OnGetSupportedVideoDecoderConfigs,
                         // base::Unretained(this) is safe because the
                         // MediaClient is never destructed.
                         base::Unretained(this)));
      video_decoder_for_supported_profiles_.emplace<
          mojo::SharedRemote<media::stable::mojom::StableVideoDecoder>>(
          std::move(stable_video_decoder_remote));
      return;
    }
    case media::OOPVDMode::kEnabledWithGpuProcessAsProxy:
    case media::OOPVDMode::kDisabled:
      break;
  }
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  mojo::SharedRemote<media::mojom::VideoDecoder> video_decoder_remote;
  interface_factory_for_supported_profiles_->CreateVideoDecoder(
      video_decoder_remote.BindNewPipeAndPassReceiver(),
      /*dst_video_decoder=*/{});
  video_decoder_remote.set_disconnect_handler(
      base::BindOnce(&RenderMediaClient::OnGetSupportedVideoDecoderConfigs,
                     // base::Unretained(this) is safe because the MediaClient
                     // is never destructed.
                     base::Unretained(this),
                     media::SupportedVideoDecoderConfigs(),
                     media::VideoDecoderType::kUnknown),
      main_task_runner_);
  video_decoder_remote->GetSupportedConfigs(
      base::BindOnce(&RenderMediaClient::OnGetSupportedVideoDecoderConfigs,
                     // base::Unretained(this) is safe because the MediaClient
                     // is never destructed.
                     base::Unretained(this)));
  video_decoder_for_supported_profiles_
      .emplace<mojo::SharedRemote<media::mojom::VideoDecoder>>(
          std::move(video_decoder_remote));
#endif
}

RenderMediaClient::~RenderMediaClient() {
  // The RenderMediaClient should never get destroyed and we have usages of
  // base::Unretained(this) here that rely on that behaviour.
  NOTREACHED_NORETURN();
}

bool RenderMediaClient::IsSupportedAudioType(const media::AudioType& type) {
  return GetContentClient()->renderer()->IsSupportedAudioType(type);
}

bool RenderMediaClient::IsSupportedVideoType(const media::VideoType& type) {
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
  if (!did_update_.IsSignaled()) {
    // The asynchronous request didn't complete in time, so we must now block
    // or retrieve the information synchronously.
    if (main_task_runner_->BelongsToCurrentThread()) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
      media::SupportedVideoDecoderConfigs configs;
      media::VideoDecoderType video_decoder_type;
      if ((absl::holds_alternative<
               mojo::SharedRemote<media::mojom::VideoDecoder>>(
               video_decoder_for_supported_profiles_) &&
           !absl::get<mojo::SharedRemote<media::mojom::VideoDecoder>>(
                video_decoder_for_supported_profiles_)
                ->GetSupportedConfigs(&configs, &video_decoder_type)) ||
          (absl::holds_alternative<
               mojo::SharedRemote<media::stable::mojom::StableVideoDecoder>>(
               video_decoder_for_supported_profiles_) &&
           !absl::get<
                mojo::SharedRemote<media::stable::mojom::StableVideoDecoder>>(
                video_decoder_for_supported_profiles_)
                ->GetSupportedConfigs(&configs, &video_decoder_type))) {
        configs.clear();
      }
      OnGetSupportedVideoDecoderConfigs(configs, video_decoder_type);
      DCHECK(did_update_.IsSignaled());
    } else {
      // There's already an asynchronous request on the main thread, so wait...
      did_update_.Wait();
    }
  }
#endif

  return GetContentClient()->renderer()->IsSupportedVideoType(type);
}

bool RenderMediaClient::IsSupportedBitstreamAudioCodec(
    media::AudioCodec codec) {
  return GetContentClient()->renderer()->IsSupportedBitstreamAudioCodec(codec);
}

std::optional<::media::AudioRendererAlgorithmParameters>
RenderMediaClient::GetAudioRendererAlgorithmParameters(
    media::AudioParameters audio_parameters) {
  return GetContentClient()->renderer()->GetAudioRendererAlgorithmParameters(
      audio_parameters);
}

void RenderMediaClient::OnGetSupportedVideoDecoderConfigs(
    const media::SupportedVideoDecoderConfigs& configs,
    media::VideoDecoderType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
  if (did_update_.IsSignaled()) {
    return;
  }

  UpdateVideoProfilesInternal(configs);
  did_update_.Signal();

  video_decoder_for_supported_profiles_
      .emplace<mojo::SharedRemote<media::mojom::VideoDecoder>>();
  interface_factory_for_supported_profiles_.reset();
#endif
}

media::ExternalMemoryAllocator* RenderMediaClient::GetMediaAllocator() {
  return GetContentClient()->renderer()->GetMediaAllocator();
}

}  // namespace content
