// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_playlist.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_refptr.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/playlist.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

// Attempts to determine playlist version from the given source (exercising
// `Playlist::IdentifyPlaylist`). Since we don't necessarily want to exit early
// on a failure here, return `kDefaultVersion` on error.
media::hls::types::DecimalInteger GetPlaylistVersion(std::string_view source) {
  auto ident_result = media::hls::Playlist::IdentifyPlaylist(source);
  if (!ident_result.has_value()) {
    return media::hls::Playlist::kDefaultVersion;
  }

  return std::move(ident_result).value().version;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);

  // Decide whether to create a multivariant playlist + media playlist or just a
  // media playlist
  scoped_refptr<media::hls::MultivariantPlaylist> multivariant_playlist;
  if (data_provider.ConsumeBool()) {
    auto multivariant_playlist_source =
        data_provider.ConsumeRandomLengthString();

    // Determine playlist version (ignore type mismatch)
    const auto version = GetPlaylistVersion(multivariant_playlist_source);
    auto multivariant_playlist_result = media::hls::MultivariantPlaylist::Parse(
        multivariant_playlist_source,
        GURL("http://localhost/multi_playlist.m3u8"), version);
    if (!multivariant_playlist_result.has_value()) {
      return 0;
    }

    multivariant_playlist = std::move(multivariant_playlist_result).value();
  }

  auto media_playlist_source = data_provider.ConsumeRemainingBytesAsString();

  // Determine playlist version (ignore type mismatch)
  const auto version = GetPlaylistVersion(media_playlist_source);
  media::hls::MediaPlaylist::Parse(media_playlist_source,
                                   GURL("http://localhost/playlist.m3u8"),
                                   version, multivariant_playlist.get());

  return 0;
}
