// Copyright 2014 The Chromium Authors. All rights reserved.
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

namespace content {

void RenderMediaClient::Initialize() {
  static RenderMediaClient* client = new RenderMediaClient();
  media::SetMediaClient(client);
}

RenderMediaClient::RenderMediaClient() {}

RenderMediaClient::~RenderMediaClient() {
}

void RenderMediaClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  GetContentClient()->renderer()->GetSupportedKeySystems(std::move(cb));
}

bool RenderMediaClient::IsSupportedAudioType(const media::AudioType& type) {
  return GetContentClient()->renderer()->IsSupportedAudioType(type);
}

bool RenderMediaClient::IsSupportedVideoType(const media::VideoType& type) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // If we're not on the render thread (ie, from the media thread), then don't
  // even bother trying to populate the cache.
  if (auto* render_thread = RenderThreadImpl::current()) {
    static const bool kHasGpuProfiles = [render_thread]() {
      if (auto gpu_host = render_thread->EstablishGpuChannelSync()) {
        const auto gpu_profiles =
            gpu_host->gpu_info().video_decode_accelerator_supported_profiles;
        base::flat_set<media::VideoCodecProfile> media_profiles;
        for (const auto& profile : gpu_profiles) {
          media_profiles.insert(
              static_cast<media::VideoCodecProfile>(profile.profile));
        }
        media::UpdateDefaultSupportedVideoProfiles(media_profiles);
        return true;
      }
      return false;
    }();
    std::ignore = kHasGpuProfiles;
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

}  // namespace content
