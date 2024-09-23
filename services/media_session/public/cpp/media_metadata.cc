// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_metadata.h"

#include "services/media_session/public/cpp/chapter_information.h"
#include "services/media_session/public/cpp/media_image.h"

namespace media_session {

MediaMetadata::MediaMetadata() = default;

MediaMetadata::~MediaMetadata() = default;

MediaMetadata::MediaMetadata(const MediaMetadata& other) = default;

bool MediaMetadata::operator==(const MediaMetadata& other) const {
  return title == other.title && artist == other.artist &&
         album == other.album && source_title == other.source_title &&
         chapters == other.chapters;
}

bool MediaMetadata::operator!=(const MediaMetadata& other) const {
  return !(*this == other);
}

bool MediaMetadata::IsEmpty() const {
  return title.empty() && artist.empty() && album.empty() &&
         source_title.empty() && chapters.empty();
}

}  // namespace media_session
