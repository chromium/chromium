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

  // Appends text to the playlist, without a trailing newline.
  void Append(base::StringPiece text) {
    source_.append(text.data(), text.size());
  }

  // Appends a new line to the playlist.
  void AppendLine(base::StringPiece line) {
    Append(line);
    Append("\n");
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

void HasType(absl::optional<PlaylistType> type,
             const base::Location& from,
             const MediaPlaylist& playlist) {
  EXPECT_EQ(playlist.GetPlaylistType(), type) << from.ToString();
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

TEST(HlsFormatParserTest, ParseMediaPlaylist_BadLineEndings) {
  TestBuilder builder;
  builder.AppendLine("#EXTM3U");

  {
    // Double carriage-return is not allowed
    auto fork = builder;
    fork.Append("\r\r\n");
    fork.ExpectError(ParseStatusCode::kInvalidEOL);
  }

  {
    // Carriage-return not followed by a newline is not allowed
    auto fork = builder;
    fork.Append("#EXT-X-VERSION:5\r");
    fork.ExpectError(ParseStatusCode::kInvalidEOL);
  }

  builder.Append("\r\n");
  builder.ExpectOk();
}

TEST(HlsFormatParserTest, ParseMediaPlaylist_MissingM3u) {
  // #EXTM3U must be the very first line
  TestBuilder builder;
  builder.AppendLine("");
  builder.AppendLine("#EXTM3U");
  builder.ExpectError(ParseStatusCode::kPlaylistMissingM3uTag);

  builder = TestBuilder();
  builder.AppendLine("#EXT-X-VERSION:5");
  builder.AppendLine("#EXTM3U");
  builder.ExpectError(ParseStatusCode::kPlaylistMissingM3uTag);

  // Test with invalid line ending
  builder = TestBuilder();
  builder.Append("#EXTM3U");
  builder.ExpectError(ParseStatusCode::kPlaylistMissingM3uTag);

  // Test with invalid format
  builder = TestBuilder();
  builder.AppendLine("#EXTM3U:");
  builder.ExpectError(ParseStatusCode::kPlaylistMissingM3uTag);
  builder = TestBuilder();
  builder.AppendLine("#EXTM3U:1");
  builder.ExpectError(ParseStatusCode::kPlaylistMissingM3uTag);

  // Extra M3U tag is OK
  builder = TestBuilder();
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXTM3U");
  builder.ExpectOk();
}

TEST(HlsFormatParserTest, ParseMediaPlaylist_UnknownTag) {
  TestBuilder builder;
  builder.AppendLine("#EXTM3U");

  // Unrecognized tags should not result in an error
  builder.AppendLine("#EXT-UNKNOWN-TAG");
  builder.ExpectOk();
}

TEST(HlsFormatParserTest, ParseMediaPlaylist_XDiscontinuityTag) {
  TestBuilder builder;
  builder.AppendLine("#EXTM3U");

  // Default discontinuity state is false
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);

  builder.AppendLine("#EXT-X-DISCONTINUITY");
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, true);

  // The discontinuity tag does not apply to subsequent segments
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);

  // The discontinuity tag may only appear once per segment
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-DISCONTINUITY");
    fork.AppendLine("#EXT-X-DISCONTINUITY");
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("video.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasDiscontinuity, true);
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }

  builder.ExpectOk();
}

TEST(HlsFormatParserTest, ParseMediaPlaylist_XGapTag) {
  TestBuilder builder;
  builder.AppendLine("#EXTM3U");

  // Default gap state is false
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(IsGap, false);

  builder.AppendLine("#EXT-X-GAP");
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(IsGap, true);

  // The gap tag does not apply to subsequent segments
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(IsGap, false);

  // The gap tag may only appear once per segment
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-GAP");
    fork.AppendLine("#EXT-X-GAP");
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("video.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(IsGap, true);
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }

  builder.ExpectOk();
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

TEST(HlsFormatParserTest, ParseMediaPlaylist_Define) {
  TestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-VERSION:8");
  builder.ExpectPlaylist(HasVersion, 8);

  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="ROOT",VALUE="http://video.com")");
  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="MOVIE",VALUE="some_video/low")");

  // Valid variable references within URI items should be substituted
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("{$ROOT}/{$MOVIE}/fileSegment0.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(
      HasUri, GURL("http://video.com/some_video/low/fileSegment0.ts"));

  {
    // Invalid variable references within URI lines should result in an error
    auto fork = builder;
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("{$root}/{$movie}/fileSegment0.ts");
    fork.ExpectError(ParseStatusCode::kVariableUndefined);
  }

  // Variable references outside of valid substitution points should not be
  // substituted
  {
    auto fork = builder;
    fork.AppendLine(R"(#EXT-X-DEFINE:NAME="LENGTH",VALUE="9.9")");
    fork.AppendLine("#EXTINF:{$LENGTH},\t");
    fork.AppendLine("http://foo/bar");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }

  // Redefinition is not allowed
  {
    auto fork = builder;
    fork.AppendLine(
        R"(#EXT-X-DEFINE:NAME="ROOT",VALUE="https://www.google.com")");
    fork.ExpectError(ParseStatusCode::kVariableDefinedMultipleTimes);
  }

  // Importing in a parentless playlist is not allowed
  {
    auto fork = builder;
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.ExpectError(ParseStatusCode::kImportedVariableInParentlessPlaylist);
  }

  // Variables may not be substituted recursively
  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="BAR",VALUE="BAZ")");
  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="FOO",VALUE="{$BAR}")");
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("http://{$FOO}.com/video");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasUri, GURL("http://{$BAR}.com/video"));

  builder.ExpectOk();
}

TEST(HlsFormatParserTest, ParseMediaPlaylist_PlaylistType) {
  TestBuilder builder;
  builder.AppendLine("#EXTM3U");

  // Without the EXT-X-PLAYLIST-TYPE tag, the playlist has no type.
  {
    auto fork = builder;
    fork.ExpectPlaylist(HasType, absl::nullopt);
    fork.ExpectOk();
  }

  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:VOD");
    fork.ExpectPlaylist(HasType, PlaylistType::kVOD);
    fork.ExpectOk();
  }

  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:EVENT");
    fork.ExpectPlaylist(HasType, PlaylistType::kEvent);
    fork.ExpectOk();
  }

  // This tag may not be specified twice
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:VOD");
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:EVENT");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:VOD");
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:VOD");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }

  // Unknown or invalid playlist types should trigger an error
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:FOOBAR");
    fork.ExpectError(ParseStatusCode::kUnknownPlaylistType);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE:");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PLAYLIST-TYPE");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }
}

}  // namespace media::hls
