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

// At present, HEVC is the only codec which has optional platform support.
// Some clients need this knowledge synchronously, so we try to populate
// it asynchronously ahead of time, but can fallback to a blocking call
// when it's needed synchronously.
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) &&                                     \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || \
     BUILDFLAG(IS_MAC))
#define NEEDS_PROFILE_UPDATER 1
#else
#define NEEDS_PROFILE_UPDATER 0
#endif

#if NEEDS_PROFILE_UPDATER
void UpdateVideoProfilesInternal(const gpu::GPUInfo& info) {
  const auto gpu_profiles = info.video_decode_accelerator_supported_profiles;
  base::flat_set<media::VideoCodecProfile> media_profiles;
  media_profiles.reserve(gpu_profiles.size());
  for (const auto& profile : gpu_profiles) {
    media_profiles.insert(
        static_cast<media::VideoCodecProfile>(profile.profile));
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

RenderMediaClient::RenderMediaClient() {
#if NEEDS_PROFILE_UPDATER
  // Unretained is safe here since the MediaClient is never destructed.
  RenderThreadImpl::current()->EstablishGpuChannel(base::BindOnce(
      &RenderMediaClient::OnEstablishedGpuChannel, base::Unretained(this)));

#endif
}

RenderMediaClient::~RenderMediaClient() = default;

void RenderMediaClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  GetContentClient()->renderer()->GetSupportedKeySystems(std::move(cb));
}

bool RenderMediaClient::IsSupportedAudioType(const media::AudioType& type) {
  return GetContentClient()->renderer()->IsSupportedAudioType(type);
}

bool RenderMediaClient::IsSupportedVideoType(const media::VideoType& type) {
#if NEEDS_PROFILE_UPDATER
  if (!did_update_.IsSignaled()) {
    // The asynchronous request didn't complete in time, so we must now block
    // until until the information from the GPU channel is available.
    if (auto* render_thread = content::RenderThreadImpl::current()) {
      if (auto gpu_host = render_thread->EstablishGpuChannelSync())
        UpdateVideoProfilesInternal(gpu_host->gpu_info());
      did_update_.Signal();
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

absl::optional<::media::AudioRendererAlgorithmParameters>
RenderMediaClient::GetAudioRendererAlgorithmParameters(
    media::AudioParameters audio_parameters) {
  return GetContentClient()->renderer()->GetAudioRendererAlgorithmParameters(
      audio_parameters);
}

void RenderMediaClient::OnEstablishedGpuChannel(
    scoped_refptr<gpu::GpuChannelHost> host) {
#if NEEDS_PROFILE_UPDATER
  if (host && !did_update_.IsSignaled())
    UpdateVideoProfilesInternal(host->gpu_info());

  // Signal even if host is nullptr, since that's the same has having no GPU.
  did_update_.Signal();
#endif
}

}  // namespace content
