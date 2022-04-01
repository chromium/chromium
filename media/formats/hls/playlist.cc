// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/playlist.h"

#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

Playlist::Playlist(GURL uri,
                   types::DecimalInteger version,
                   bool independent_segments)
    : uri_(std::move(uri)),
      version_(version),
      independent_segments_(independent_segments) {}

Playlist::Playlist(Playlist&&) = default;
Playlist& Playlist::operator=(Playlist&&) = default;
Playlist::~Playlist() = default;

}  // namespace media::hls
