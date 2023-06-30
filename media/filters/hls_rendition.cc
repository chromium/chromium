// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_rendition.h"

#include "media/filters/hls_live_rendition.h"
#include "media/filters/hls_vod_rendition.h"
#include "media/filters/manifest_demuxer.h"

namespace media {

namespace {

absl::optional<base::TimeDelta> GetPlaylistDuration(
    hls::MediaPlaylist* playlist) {
  if (!playlist->HasMediaSequenceTag()) {
    // Live playbacks have a media sequence tag, so if that's missing, then this
    // playback is VOD, and we can use it's computed duration.
    return playlist->GetComputedDuration();
  }

  auto segments = playlist->GetSegments();
  // Usually manifests use an Endlist tag to end a live playback, but its
  // also fairly common to see these on VOD content where the first media
  // sequence is id 0 or 1.
  if (playlist->IsEndList()) {
    if (!segments.empty() && segments[0]->GetMediaSequenceNumber() < 2) {
      return playlist->GetComputedDuration();
    }
  }

  // Live content doesn't have a defined duration.
  return absl::nullopt;
}

}  // namespace

// Static
HlsDemuxerStatus::Or<std::unique_ptr<HlsRendition>>
HlsRendition::CreateRendition(ManifestDemuxerEngineHost* engine_host,
                              HlsRenditionHost* rendition_host,
                              std::string role,
                              scoped_refptr<hls::MediaPlaylist> playlist,
                              GURL uri) {
  std::unique_ptr<HlsRendition> rendition;
  auto duration = GetPlaylistDuration(playlist.get());
  if (duration.has_value()) {
    rendition = std::make_unique<HlsVodRendition>(
        engine_host, rendition_host, std::move(role), std::move(playlist),
        duration.value());
  } else {
    rendition = std::make_unique<HlsLiveRendition>(
        engine_host, rendition_host, role, std::move(playlist), uri);
  }
  return rendition;
}

}  // namespace media
