// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_rendition.h"

#include "media/filters/manifest_demuxer.h"

namespace media {

// Static
HlsDemuxerStatus::Or<std::unique_ptr<HlsRendition>>
HlsRendition::CreateRendition(ManifestDemuxerEngineHost* engine_host,
                              HlsRenditionHost* rendition_host,
                              std::string role,
                              scoped_refptr<hls::MediaPlaylist> playlist,
                              GURL uri) {
  return HlsDemuxerStatus::Codes::kInvalidManifest;
}

}  // namespace media
