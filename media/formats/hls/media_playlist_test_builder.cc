// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_playlist_test_builder.h"

#include "base/location.h"

namespace media::hls {

MediaPlaylistTestBuilder::MediaPlaylistTestBuilder() = default;

MediaPlaylistTestBuilder::~MediaPlaylistTestBuilder() = default;

MediaPlaylistTestBuilder::MediaPlaylistTestBuilder(
    const MediaPlaylistTestBuilder&) = default;

MediaPlaylistTestBuilder::MediaPlaylistTestBuilder(MediaPlaylistTestBuilder&&) =
    default;

MediaPlaylistTestBuilder& MediaPlaylistTestBuilder::operator=(
    const MediaPlaylistTestBuilder&) = default;

MediaPlaylistTestBuilder& MediaPlaylistTestBuilder::operator=(
    MediaPlaylistTestBuilder&&) = default;

MediaPlaylistTestBuilder::SegmentExpectations::SegmentExpectations() = default;

MediaPlaylistTestBuilder::SegmentExpectations::~SegmentExpectations() = default;

MediaPlaylistTestBuilder::SegmentExpectations::SegmentExpectations(
    const SegmentExpectations&) = default;

MediaPlaylistTestBuilder::SegmentExpectations::SegmentExpectations(
    SegmentExpectations&&) = default;

MediaPlaylistTestBuilder::SegmentExpectations&
MediaPlaylistTestBuilder::SegmentExpectations::operator=(
    const SegmentExpectations&) = default;

MediaPlaylistTestBuilder::SegmentExpectations&
MediaPlaylistTestBuilder::SegmentExpectations::operator=(
    SegmentExpectations&&) = default;

void MediaPlaylistTestBuilder::VerifyExpectations(
    const MediaPlaylist& playlist,
    const base::Location& from) const {
  ASSERT_EQ(segment_expectations_.size(), playlist.GetSegments().size())
      << from.ToString();
  for (size_t i = 0; i < segment_expectations_.size(); ++i) {
    const auto& segment = playlist.GetSegments().at(i);
    const auto& expectations = segment_expectations_.at(i);
    for (const auto& expectation : expectations.expectations) {
      expectation.Run(*segment);
    }
  }
}

}  // namespace media::hls
