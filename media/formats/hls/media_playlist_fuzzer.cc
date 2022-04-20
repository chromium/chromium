// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <cstddef>
#include <cstdint>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/i18n/icu_util.h"
#include "base/strings/string_piece.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);

  // Decide whether to create a multivariant playlist + media playlist or just a
  // media playlist
  std::unique_ptr<media::hls::MultivariantPlaylist> multivariant_playlist;
  if (data_provider.ConsumeBool()) {
    auto multivariant_playlist_source =
        data_provider.ConsumeRandomLengthString();
    auto multivariant_playlist_result = media::hls::MultivariantPlaylist::Parse(
        multivariant_playlist_source,
        GURL("http://localhost/multi_playlist.m3u8"));
    if (multivariant_playlist_result.has_error()) {
      return 0;
    }

    multivariant_playlist = std::make_unique<media::hls::MultivariantPlaylist>(
        std::move(multivariant_playlist_result).value());
  }

  auto media_playlist_source = data_provider.ConsumeRemainingBytesAsString();
  media::hls::MediaPlaylist::Parse(media_playlist_source,
                                   GURL("http://localhost/playlist.m3u8"),
                                   multivariant_playlist.get());

  return 0;
}
