// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_playlist.h"

#include <vector>

#include "base/callback_list.h"
#include "base/location.h"
#include "media/formats/hls/items.h"
#include "media/formats/hls/media_segment.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

namespace {

class TestBuilder {
 public:
  void SetUri(GURL uri) { uri_ = std::move(uri); }

  // Appends a new line to the playlist.
  void AppendLine(base::StringPiece line) {
    source_.append(line.data(), line.size());
    source_.append("\n");
  }

  // Adds a new expectation for the playlist, which will be checked during
  // `ExpectOk`.
  template <typename Fn, typename Arg>
  void ExpectPlaylist(Fn fn,
                      Arg arg,
                      base::Location location = base::Location::Current()) {
    playlist_expectations_.emplace_back(base::BindRepeating(
        std::move(fn), std::move(arg), std::move(location)));
  }

  // Increments the number of segments that are expected to be contained in the
  // playlist.
  void ExpectAdditionalSegment() { segment_expectations_.push_back({}); }

  // Adds a new expectation for the latest segment in the playlist, which will
  // be checked during `ExpectOk`.
  template <typename Fn, typename Arg>
  void ExpectSegment(Fn fn,
                     Arg arg,
                     base::Location location = base::Location::Current()) {
    segment_expectations_.back().expectations.emplace_back(base::BindRepeating(
        std::move(fn), std::move(arg), std::move(location)));
  }

  // Attempts to parse the playlist as-is, checking for the given
  // error code.
  void ExpectError(
      ParseStatusCode code,
      const base::Location& from = base::Location::Current()) const {
    auto result = MediaPlaylist::Parse(source_, uri_);
    ASSERT_TRUE(result.has_error()) << from.ToString();
    EXPECT_EQ(std::move(result).error().code(), code) << from.ToString();
  }

  // Attempts to parse the playlist as-is, checking all playlist and segment
  // expectations.
  void ExpectOk(const base::Location& from = base::Location::Current()) const {
    auto result = MediaPlaylist::Parse(source_, uri_);
    ASSERT_TRUE(result.has_value())
        << "Error: "
        << ParseStatusCodeToString(std::move(result).error().code()) << "\n"
        << from.ToString();
    auto playlist = std::move(result).value();

    for (const auto& expectation : playlist_expectations_) {
      expectation.Run(playlist);
    }

    ASSERT_EQ(segment_expectations_.size(), playlist.GetSegments().size())
        << from.ToString();
    for (size_t i = 0; i < segment_expectations_.size(); ++i) {
      const auto& segment = playlist.GetSegments().at(i);
      const auto& expectations = segment_expectations_.at(i);
      for (const auto& expectation : expectations.expectations) {
        expectation.Run(segment);
      }
    }
  }

 private:
  struct SegmentExpectations {
    std::vector<base::RepeatingCallback<void(const MediaSegment&)>>
        expectations;
  };

  std::vector<SegmentExpectations> segment_expectations_;
  std::vector<base::RepeatingCallback<void(const MediaPlaylist&)>>
      playlist_expectations_;
  GURL uri_ = GURL("http://localhost/playlist.m3u8");
  std::string source_;
};

void HasVersion(types::DecimalInteger version,
                const base::Location& from,
                const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.GetVersion(), version) << from.ToString();
}

void HasDuration(types::DecimalFloatingPoint duration,
                 const base::Location& from,
                 const MediaSegment& segment) {
  EXPECT_DOUBLE_EQ(segment.GetDuration(), duration) << from.ToString();
}

void HasUri(GURL uri, const base::Location& from, const MediaSegment& segment) {
  EXPECT_EQ(segment.GetUri(), uri) << from.ToString();
}

void HasDiscontinuity(bool value,
                      const base::Location& from,
                      const MediaSegment& segment) {
  EXPECT_EQ(segment.HasDiscontinuity(), value) << from.ToString();
}

void IsGap(bool value,
           const base::Location& from,
           const MediaSegment& segment) {
  EXPECT_EQ(segment.IsGap(), value) << from.ToString();
}

}  // namespace

TEST(HlsFormatParserTest, ParseMediaPlaylist_MissingM3u) {
  TestBuilder builder;
  builder.AppendLine("#EXT-X-VERSION:5");
  builder.ExpectError(ParseStatusCode::kPlaylistMissingM3uTag);
}

TEST(HlsFormatParserTest, ParseMediaPlaylist_VersionChecks) {
  TestBuilder builder;
  builder.AppendLine("#EXTM3U");

  {
    // Default version is 1
    auto fork = builder;
    fork.ExpectPlaylist(HasVersion, 1);
    fork.ExpectOk();
  }

  {
    // "-1" is not a valid decimal-integer
    auto fork = builder;
    fork.AppendLine("#EXT-X-VERSION:-1");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }

  {
    // "0" is not a valid version
    auto fork = builder;
    fork.AppendLine("#EXT-X-VERSION:0");
    fork.ExpectError(ParseStatusCode::kInvalidPlaylistVersion);
  }

  for (int i = 1; i <= 10; ++i) {
    auto fork = builder;
    fork.AppendLine("#EXT-X-VERSION:" + base::NumberToString(i));
    fork.ExpectPlaylist(HasVersion, i);
    fork.ExpectOk();
  }

  for (int i : {11, 12, 100, 999}) {
    // Versions 11+ are not supported by this parser
    auto fork = builder;
    fork.AppendLine("#EXT-X-VERSION:" + base::NumberToString(i));
    fork.ExpectError(ParseStatusCode::kPlaylistHasUnsupportedVersion);
  }
}

TEST(HlsFormatParserTest, ParseMediaPlaylist_Segments) {
  TestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-VERSION:5");
  builder.ExpectPlaylist(HasVersion, 5);

  builder.AppendLine("#EXTINF:9.2,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);
  builder.ExpectSegment(HasDuration, 9.2);
  builder.ExpectSegment(HasUri, GURL("http://localhost/video.ts"));
  builder.ExpectSegment(IsGap, false);

  // Segments without #EXTINF tags are not allowed
  {
    auto fork = builder;
    fork.AppendLine("foobar.ts");
    fork.ExpectError(ParseStatusCode::kMediaSegmentMissingInfTag);
  }

  builder.AppendLine("#EXTINF:9.3,foo");
  builder.AppendLine("#EXT-X-DISCONTINUITY");
  builder.AppendLine("foo.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, true);
  builder.ExpectSegment(HasDuration, 9.3);
  builder.ExpectSegment(IsGap, false);
  builder.ExpectSegment(HasUri, GURL("http://localhost/foo.ts"));

  builder.AppendLine("#EXTINF:9.2,bar");
  builder.AppendLine("http://foo/bar.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);
  builder.ExpectSegment(HasDuration, 9.2);
  builder.ExpectSegment(IsGap, false);
  builder.ExpectSegment(HasUri, GURL("http://foo/bar.ts"));

  builder.ExpectOk();
}

}  // namespace media::hls
