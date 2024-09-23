// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/multivariant_playlist.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/i18n/icu_util.h"
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
  // Create a string_view from the given input
  const std::string_view source(reinterpret_cast<const char*>(data), size);

  // Determine playlist version (ignoring type mismatch)
  const auto version = GetPlaylistVersion(source);

  // Try to parse it as a multivariant playlist
  media::hls::MultivariantPlaylist::Parse(
      source, GURL("http://localhost/playlist.m3u8"), version);

  return 0;
}
