// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_TEST_BUILDER_H_
#define MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_TEST_BUILDER_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/time/time.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/media_segment.h"
#include "media/formats/hls/playlist_test_builder.h"

namespace media::hls {

class MultivariantPlaylist;

// Helper for building media playlist test cases that allows writing assertions
// next to the playlist lines they check, as well as "forking" test cases via
// copying the builder.
class MediaPlaylistTestBuilder : public PlaylistTestBuilder<MediaPlaylist> {
 public:
  MediaPlaylistTestBuilder();
  ~MediaPlaylistTestBuilder();
  MediaPlaylistTestBuilder(const MediaPlaylistTestBuilder&);
  MediaPlaylistTestBuilder(MediaPlaylistTestBuilder&&);
  MediaPlaylistTestBuilder& operator=(const MediaPlaylistTestBuilder&);
  MediaPlaylistTestBuilder& operator=(MediaPlaylistTestBuilder&&);

  // Sets the referring multivariant playlist.
  void SetParent(const MultivariantPlaylist* parent) { parent_ = parent; }

  // Increments the number of segments that are expected to be contained in the
  // playlist.
  void ExpectAdditionalSegment() { segment_expectations_.emplace_back(); }

  // Adds a new expectation for the latest segment in the playlist, which will
  // be checked during `ExpectOk`.
  template <typename Fn, typename Arg>
  void ExpectSegment(Fn fn,
                     Arg arg,
                     base::Location location = base::Location::Current()) {
    segment_expectations_.back().expectations.push_back(base::BindRepeating(
        std::move(fn), std::move(arg), std::move(location)));
  }

  void ExpectOk(const base::Location& from = base::Location::Current()) const {
    PlaylistTestBuilder::ExpectOk(from, parent_);
  }

  void ExpectError(
      ParseStatusCode code,
      const base::Location& from = base::Location::Current()) const {
    PlaylistTestBuilder::ExpectError(code, from, parent_);
  }

 private:
  struct SegmentExpectations {
    SegmentExpectations();
    ~SegmentExpectations();
    SegmentExpectations(const SegmentExpectations&);
    SegmentExpectations(SegmentExpectations&&);
    SegmentExpectations& operator=(const SegmentExpectations&);
    SegmentExpectations& operator=(SegmentExpectations&&);

    std::vector<base::RepeatingCallback<void(const MediaSegment&)>>
        expectations;
  };

  void VerifyExpectations(const MediaPlaylist& playlist,
                          const base::Location& from) const override;

  const MultivariantPlaylist* parent_ = nullptr;
  std::vector<SegmentExpectations> segment_expectations_;
};

// Checks that the media playlist has the given type (or `absl::nullopt`).
inline void HasType(absl::optional<PlaylistType> type,
                    const base::Location& from,
                    const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.GetPlaylistType(), type) << from.ToString();
}

// Checks that the media playlist has the given Target Duration.
inline void HasTargetDuration(base::TimeDelta value,
                              const base::Location& from,
                              const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.GetTargetDuration(), value) << from.ToString();
}

// Checks that the value of `GetComputedDuration()` matches the given value.
inline void HasComputedDuration(base::TimeDelta value,
                                const base::Location& from,
                                const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.GetComputedDuration(), value) << from.ToString();
}

// Checks the media playlist's `HasMediaSequenceTag` property against
// the given value.
inline void HasMediaSequenceTag(bool value,
                                const base::Location& from,
                                const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.HasMediaSequenceTag(), value) << from.ToString();
}

// Checks that the latest media segment has the given duration.
inline void HasDuration(types::DecimalFloatingPoint duration,
                        const base::Location& from,
                        const MediaSegment& segment) {
  EXPECT_DOUBLE_EQ(segment.GetDuration(), duration) << from.ToString();
}

// Checks that the latest media segment has the given media sequence number.
inline void HasMediaSequenceNumber(types::DecimalInteger number,
                                   const base::Location& from,
                                   const MediaSegment& segment) {
  EXPECT_EQ(segment.GetMediaSequenceNumber(), number) << from.ToString();
}

// Checks that the latest media segment has the given URI.
inline void HasUri(GURL uri,
                   const base::Location& from,
                   const MediaSegment& segment) {
  EXPECT_EQ(segment.GetUri(), uri) << from.ToString();
}

// Checks the latest media segment's `HasDiscontinuity` property against the
// given value.
inline void HasDiscontinuity(bool value,
                             const base::Location& from,
                             const MediaSegment& segment) {
  EXPECT_EQ(segment.HasDiscontinuity(), value) << from.ToString();
}

// Checks the latest media segment's `IsGap` property against the given value.
inline void IsGap(bool value,
                  const base::Location& from,
                  const MediaSegment& segment) {
  EXPECT_EQ(segment.IsGap(), value) << from.ToString();
}

// Checks the value of `IsEndList` against the given value.
inline void IsEndList(bool value,
                      const base::Location& from,
                      const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.IsEndList(), value) << from.ToString();
}

// Checks the value of `IsIFramesOnly` against the given value.
inline void IsIFramesOnly(bool value,
                          const base::Location& from,
                          const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.IsIFramesOnly(), value) << from.ToString();
}

}  // namespace media::hls

#endif
