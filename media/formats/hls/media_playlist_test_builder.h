// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_TEST_BUILDER_H_
#define MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_TEST_BUILDER_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/media_segment.h"
#include "media/formats/hls/playlist_test_builder.h"
#include "media/formats/hls/test_util.h"
#include "media/formats/hls/types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

  raw_ptr<const MultivariantPlaylist, DanglingUntriaged> parent_ = nullptr;
  std::vector<SegmentExpectations> segment_expectations_;
};

// Checks that the media playlist has the given type (or `std::nullopt`).
inline void HasType(std::optional<PlaylistType> type,
                    const base::Location& from,
                    const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.GetPlaylistType(), type) << from.ToString();
}

// Checks that the media playlist has the given Target Duration.
inline void HasTargetDuration(base::TimeDelta value,
                              const base::Location& from,
                              const MediaPlaylist& playlist) {
  EXPECT_TRUE(RoughlyEqual(playlist.GetTargetDuration(), value))
      << from.ToString();
}

// Checks that the value of `GetComputedDuration()` matches the given value.
inline void HasComputedDuration(base::TimeDelta value,
                                const base::Location& from,
                                const MediaPlaylist& playlist) {
  EXPECT_TRUE(RoughlyEqual(playlist.GetComputedDuration(), value))
      << from.ToString();
}

// Checks that the value of `GetPartialSegmentInfo()` matches the given value.
inline void HasPartialSegmentInfo(
    std::optional<MediaPlaylist::PartialSegmentInfo> partial_segment_info,
    const base::Location& from,
    const MediaPlaylist& playlist) {
  ASSERT_EQ(partial_segment_info.has_value(),
            playlist.GetPartialSegmentInfo().has_value())
      << from.ToString();
  if (partial_segment_info.has_value()) {
    EXPECT_TRUE(RoughlyEqual(partial_segment_info->target_duration,
                             playlist.GetPartialSegmentInfo()->target_duration))
        << from.ToString();
  }
}

// Checks the media playlist's `HasMediaSequenceTag` property against
// the given value.
inline void HasMediaSequenceTag(bool value,
                                const base::Location& from,
                                const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.HasMediaSequenceTag(), value) << from.ToString();
}

// Checks that the value of `GetSkipBoundary()` matches the given value.
inline void HasSkipBoundary(std::optional<base::TimeDelta> value,
                            const base::Location& from,
                            const MediaPlaylist& playlist) {
  EXPECT_TRUE(RoughlyEqual(playlist.GetSkipBoundary(), value))
      << from.ToString();
}

// Checks that the value of `CanSkipDateRanges()` matches the given value.
inline void CanSkipDateRanges(bool value,
                              const base::Location& from,
                              const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.CanSkipDateRanges(), value) << from.ToString();
}

// Checks that the value of `GetHoldBackDistance()` matches the given value.
inline void HasHoldBackDistance(base::TimeDelta value,
                                const base::Location& from,
                                const MediaPlaylist& playlist) {
  EXPECT_TRUE(RoughlyEqual(playlist.GetHoldBackDistance(), value))
      << from.ToString();
}

// Checks that the value of `GetPartHoldBackDistance()` matches the given value.
inline void HasPartHoldBackDistance(std::optional<base::TimeDelta> value,
                                    const base::Location& from,
                                    const MediaPlaylist& playlist) {
  EXPECT_TRUE(RoughlyEqual(playlist.GetPartHoldBackDistance(), value))
      << from.ToString();
}

// Checks that the value of `CanBlockReload()` matches the given value.
inline void CanBlockReload(bool value,
                           const base::Location& from,
                           const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.CanBlockReload(), value) << from.ToString();
}

// Checks that the latest media segment has the given duration.
inline void HasDuration(base::TimeDelta duration,
                        const base::Location& from,
                        const MediaSegment& segment) {
  EXPECT_TRUE(RoughlyEqual(segment.GetDuration(), duration)) << from.ToString();
}

// Checks that the latest media segment has the given media sequence number.
inline void HasMediaSequenceNumber(types::DecimalInteger number,
                                   const base::Location& from,
                                   const MediaSegment& segment) {
  EXPECT_EQ(segment.GetMediaSequenceNumber(), number) << from.ToString();
}

// Checks that the latest media segment has the given media sequence number.
inline void HasEncryptionData(
    std::optional<std::tuple<GURL,
                             XKeyTagMethod,
                             XKeyTagKeyFormat,
                             MediaSegment::EncryptionData::IVContainer>> pack,
    const base::Location& from,
    const MediaSegment& segment) {
  auto enc_data = segment.GetEncryptionData();
  if (!pack.has_value()) {
    ASSERT_EQ(enc_data.get(), nullptr) << from.ToString();
  } else {
    ASSERT_NE(enc_data.get(), nullptr) << from.ToString();
    GURL uri;
    XKeyTagMethod method;
    XKeyTagKeyFormat format;
    MediaSegment::EncryptionData::IVContainer iv;
    std::tie(uri, method, format, iv) = pack.value();
    EXPECT_EQ(enc_data->GetUri(), uri) << from.ToString();
    EXPECT_EQ(enc_data->GetMethod(), method) << from.ToString();
    EXPECT_EQ(enc_data->GetKeyFormat(), format) << from.ToString();
    EXPECT_EQ(enc_data->GetIV(segment.GetMediaSequenceNumber()), iv)
        << from.ToString();
  }
}

// Checks that the latest media segment has the given discontinuity sequence
// number.
inline void HasDiscontinuitySequenceNumber(types::DecimalInteger number,
                                           const base::Location& from,
                                           const MediaSegment& segment) {
  EXPECT_EQ(segment.GetDiscontinuitySequenceNumber(), number)
      << from.ToString();
}

// Checks that the latest media segment has the given URI.
inline void HasUri(GURL uri,
                   const base::Location& from,
                   const MediaSegment& segment) {
  EXPECT_EQ(segment.GetUri(), uri) << from.ToString();
}

// Checks that the latest media segment's media initialization segment is
// equivalent to the given value.
inline void HasInitializationSegment(
    scoped_refptr<MediaSegment::InitializationSegment> expected,
    const base::Location& from,
    const MediaSegment& segment) {
  auto actual = segment.GetInitializationSegment();
  if (actual && expected) {
    EXPECT_EQ(actual->GetUri(), expected->GetUri()) << from.ToString();

    if (actual->GetByteRange() && expected->GetByteRange()) {
      EXPECT_EQ(actual->GetByteRange()->GetOffset(),
                expected->GetByteRange()->GetOffset())
          << from.ToString();
      EXPECT_EQ(actual->GetByteRange()->GetLength(),
                expected->GetByteRange()->GetLength())
          << from.ToString();
      EXPECT_EQ(actual->GetByteRange()->GetEnd(),
                expected->GetByteRange()->GetEnd())
          << from.ToString();
    } else {
      EXPECT_FALSE(actual->GetByteRange() || expected->GetByteRange())
          << from.ToString();
    }
  } else {
    EXPECT_FALSE(actual || expected) << from.ToString();
  }
}

// Checks that the latest media segment has the given byte range.
inline void HasByteRange(std::optional<types::ByteRange> range,
                         const base::Location& from,
                         const MediaSegment& segment) {
  ASSERT_EQ(segment.GetByteRange().has_value(), range.has_value())
      << from.ToString();
  if (range.has_value()) {
    EXPECT_EQ(segment.GetByteRange()->GetOffset(), range->GetOffset())
        << from.ToString();
    EXPECT_EQ(segment.GetByteRange()->GetLength(), range->GetLength())
        << from.ToString();
    EXPECT_EQ(segment.GetByteRange()->GetEnd(), range->GetEnd())
        << from.ToString();
  }
}

// Checks the latest media segment's `GetBitRate` property against the given
// value.
inline void HasBitRate(std::optional<types::DecimalInteger> bitrate,
                       const base::Location& from,
                       const MediaSegment& segment) {
  EXPECT_EQ(segment.GetBitRate(), bitrate);
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

#endif  // MEDIA_FORMATS_HLS_MEDIA_PLAYLIST_TEST_BUILDER_H_
