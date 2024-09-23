// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_playlist.h"

#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "media/formats/hls/media_playlist_test_builder.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/playlist.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media::hls {

namespace {

scoped_refptr<MultivariantPlaylist> CreateMultivariantPlaylist(
    std::initializer_list<std::string_view> lines,
    GURL uri = GURL("http://localhost/multi_playlist.m3u8"),
    types::DecimalInteger version = Playlist::kDefaultVersion) {
  std::string source;
  for (auto line : lines) {
    source.append(line);
    source.append("\n");
  }

  // Parse the given source. Failure here isn't supposed to be part of the test,
  // so use a CHECK.
  auto result = MultivariantPlaylist::Parse(source, std::move(uri), version);
  CHECK(result.has_value());
  return std::move(result).value();
}

}  // namespace

TEST(HlsMediaPlaylistTest, Segments) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.AppendLine("#EXT-X-VERSION:5");
  builder.SetVersion(5);
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(10));

  builder.AppendLine("#EXTINF:9.2,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);
  builder.ExpectSegment(HasDuration, base::Seconds(9.2));
  builder.ExpectSegment(HasUri, GURL("http://localhost/video.ts"));
  builder.ExpectSegment(IsGap, false);
  builder.ExpectSegment(HasMediaSequenceNumber, 0);

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
  builder.ExpectSegment(HasDuration, base::Seconds(9.3));
  builder.ExpectSegment(IsGap, false);
  builder.ExpectSegment(HasUri, GURL("http://localhost/foo.ts"));
  builder.ExpectSegment(HasMediaSequenceNumber, 1);

  builder.AppendLine("#EXTINF:9.2,bar");
  builder.AppendLine("http://foo/bar.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);
  builder.ExpectSegment(HasDuration, base::Seconds(9.2));
  builder.ExpectSegment(IsGap, false);
  builder.ExpectSegment(HasUri, GURL("http://foo/bar.ts"));
  builder.ExpectSegment(HasMediaSequenceNumber, 2);

  // Segments must not exceed the playlist's target duration when rounded to the
  // nearest integer
  {
    auto fork = builder;
    fork.AppendLine("#EXTINF:10.499,bar");
    fork.AppendLine("bar.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectOk();

    fork.AppendLine("#EXTINF:10.5,baz");
    fork.AppendLine("baz.ts");
    fork.ExpectError(ParseStatusCode::kMediaSegmentExceedsTargetDuration);
  }

  builder.ExpectOk();
}

TEST(HlsMediaPlaylistTest, TotalDuration) {
  constexpr types::DecimalInteger kSegmentDuration =
      MediaPlaylist::kMaxTargetDuration.InSeconds();
  constexpr size_t kMaxSegments =
      base::TimeDelta::FiniteMax().InSeconds() / kSegmentDuration;

  // Make sure this test won't take an unreasonable amount of time to run
  static_assert(kMaxSegments < 1000);

  // Ensure that if we have a playlist large enough where the total duration
  // overflows `base::TimeDelta`, this is caught.
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:" +
                     base::NumberToString(kSegmentDuration));
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(kSegmentDuration));

  for (size_t i = 0; i < kMaxSegments; ++i) {
    builder.AppendLine("#EXTINF:" + base::NumberToString(kSegmentDuration) +
                       ",\t");
    builder.AppendLine("segment" + base::NumberToString(i) + ".ts");
    builder.ExpectAdditionalSegment();
    builder.ExpectSegment(HasDuration, base::Seconds(kSegmentDuration));
    builder.ExpectSegment(HasUri, GURL("http://localhost/segment" +
                                       base::NumberToString(i) + ".ts"));
  }

  // The segments above should not overflow the playlist duration
  builder.ExpectPlaylist(HasComputedDuration,
                         base::Seconds(kSegmentDuration * kMaxSegments));
  builder.ExpectOk();

  // But an additional segment would
  builder.AppendLine("#EXTINF:" + base::NumberToString(kSegmentDuration) +
                     ",\t");
  builder.AppendLine("segmentX.ts");
  builder.ExpectError(ParseStatusCode::kPlaylistOverflowsTimeDelta);
}

// This test is similar to the `HlsMultivariantPlaylistTest` test of the same
// name, but due to subtle differences between media playlists and multivariant
// playlists its difficult to combine them. If new cases are added here that are
// also relevant to multivariant playlists, they should be added to that test as
// well.
TEST(HlsMediaPlaylistTest, VariableSubstitution) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.AppendLine("#EXT-X-VERSION:8");
  builder.SetVersion(8);
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(10));

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

  // Test importing variables in a playlist with a parent
  auto parent = CreateMultivariantPlaylist(
      {"#EXTM3U", "#EXT-X-VERSION:8",
       R"(#EXT-X-DEFINE:NAME="IMPORTED",VALUE="HELLO")"},
      GURL("http://localhost/multi_playlist.m3u8"), 8);
  {
    // Referring to a parent playlist variable without importing it is an error
    auto fork = builder;
    fork.SetParent(parent.get());
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("segments/{$IMPORTED}.ts");
    fork.ExpectError(ParseStatusCode::kVariableUndefined);
  }
  {
    // Locally overwriting an unimported variable from a parent playlist is NOT
    // an error
    auto fork = builder;
    fork.SetParent(parent.get());
    fork.AppendLine(R"(#EXT-X-DEFINE:NAME="IMPORTED",VALUE="WORLD")");
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("segments/{$IMPORTED}.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segments/WORLD.ts"));
    fork.ExpectOk();

    // Importing a variable once it's been defined is an error
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.ExpectError(ParseStatusCode::kVariableDefinedMultipleTimes);
  }
  {
    // Defining a variable once it's been imported is an error
    auto fork = builder;
    fork.SetParent(parent.get());
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.AppendLine(R"(#EXT-X-DEFINE:NAME="IMPORTED",VALUE="WORLD")");
    fork.ExpectError(ParseStatusCode::kVariableDefinedMultipleTimes);
  }
  {
    // Importing the same variable twice is an error
    auto fork = builder;
    fork.SetParent(parent.get());
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.ExpectError(ParseStatusCode::kVariableDefinedMultipleTimes);
  }
  {
    // Importing a variable that hasn't been defined in the parent playlist is
    // an error
    auto fork = builder;
    fork.SetParent(parent.get());
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="FOO")");
    fork.ExpectError(ParseStatusCode::kImportedVariableUndefined);
  }
  {
    // Test actually using an imported variable
    auto fork = builder;
    fork.SetParent(parent.get());
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.AppendLine("#EXTINF:9.9,\t");
    fork.AppendLine("segments/{$IMPORTED}.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segments/HELLO.ts"));
    fork.ExpectOk();
  }

  // Variables are not resolved recursively
  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="BAR",VALUE="BAZ")");
  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="FOO",VALUE="{$BAR}")");
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("http://{$FOO}.com/video");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasUri, GURL("http://{$BAR}.com/video"));

  builder.ExpectOk();
}

TEST(HlsMediaPlaylistTest, MultivariantPlaylistTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // Media playlists may not contain tags exclusive to multivariant playlists
  for (TagName name = ToTagName(MultivariantPlaylistTagName::kMinValue);
       name <= ToTagName(MultivariantPlaylistTagName::kMaxValue); ++name) {
    auto tag_line = "#" + std::string{TagNameToString(name)};
    auto fork = builder;
    fork.AppendLine(tag_line);
    fork.ExpectError(ParseStatusCode::kMediaPlaylistHasMultivariantPlaylistTag);
  }
}

TEST(HlsMediaPlaylistTest, XIndependentSegmentsTagInParent) {
  auto parent1 = CreateMultivariantPlaylist({
      "#EXTM3U",
      "#EXT-X-INDEPENDENT-SEGMENTS",
  });

  // Parent value should carryover to media playlist
  MediaPlaylistTestBuilder builder;
  builder.SetParent(parent1.get());
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.ExpectPlaylist(HasIndependentSegments, true);
  builder.ExpectOk();

  // It's OK for this tag to reappear in the media playlist
  builder.AppendLine("#EXT-X-INDEPENDENT-SEGMENTS");
  builder.ExpectOk();

  // Without that tag in the parent, the value depends entirely on its presence
  // in the child
  auto parent2 = CreateMultivariantPlaylist({"#EXTM3U"});
  builder = MediaPlaylistTestBuilder();
  builder.SetParent(parent2.get());
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  {
    auto fork = builder;
    fork.ExpectPlaylist(HasIndependentSegments, false);
    fork.ExpectOk();
  }
  builder.AppendLine("#EXT-X-INDEPENDENT-SEGMENTS");
  builder.ExpectPlaylist(HasIndependentSegments, true);
  builder.ExpectOk();
  EXPECT_FALSE(parent2->AreSegmentsIndependent());
}

TEST(HlsMediaPlaylistTest, XBitrateTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // The EXT-X-BITRATE tag must be a valid DecimalInteger
  {
    for (std::string_view x : {"", ":", ": 1", ":1 ", ":-1", ":{$bitrate}"}) {
      auto fork = builder;
      fork.AppendLine("#EXT-X-BITRATE", x);
      fork.ExpectError(ParseStatusCode::kMalformedTag);
    }
  }

  // The EXT-X-BITRATE tag applies only to the segments that it appears after
  builder.AppendLine("#EXTINF:9.2,");
  builder.AppendLine("segment0.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasMediaSequenceNumber, 0);
  builder.ExpectSegment(HasUri, GURL("http://localhost/segment0.ts"));
  builder.ExpectSegment(HasBitRate, std::nullopt);

  builder.AppendLine("#EXT-X-BITRATE:15");
  builder.AppendLine("#EXTINF:9.2,");
  builder.AppendLine("segment1.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasMediaSequenceNumber, 1);
  builder.ExpectSegment(HasUri, GURL("http://localhost/segment1.ts"));
  builder.ExpectSegment(HasBitRate, 15000);

  builder.AppendLine("#EXTINF:9.2,");
  builder.AppendLine("segment2.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasMediaSequenceNumber, 2);
  builder.ExpectSegment(HasUri, GURL("http://localhost/segment2.ts"));
  builder.ExpectSegment(HasBitRate, 15000);

  // The EXT-X-BITRATE tag does not apply to segments that are byteranges
  builder.AppendLine("#EXT-X-BYTERANGE:1024@0");
  builder.AppendLine("#EXTINF:9.2,");
  builder.AppendLine("segment3.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasMediaSequenceNumber, 3);
  builder.ExpectSegment(HasUri, GURL("http://localhost/segment3.ts"));
  builder.ExpectSegment(HasByteRange, CreateByteRange(1024, 0));
  builder.ExpectSegment(HasBitRate, std::nullopt);

  builder.AppendLine("#EXTINF:9.2,");
  builder.AppendLine("segment4.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasMediaSequenceNumber, 4);
  builder.ExpectSegment(HasUri, GURL("http://localhost/segment4.ts"));
  builder.ExpectSegment(HasByteRange, std::nullopt);
  builder.ExpectSegment(HasBitRate, 15000);

  // The EXT-X-BITRATE tag is allowed to appear twice
  builder.AppendLine("#EXT-X-BITRATE:20");
  builder.AppendLine("#EXT-X-BITRATE:21");
  builder.AppendLine("#EXTINF:9.2,");
  builder.AppendLine("segment5.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasMediaSequenceNumber, 5);
  builder.ExpectSegment(HasUri, GURL("http://localhost/segment5.ts"));
  builder.ExpectSegment(HasBitRate, 21000);

  // A value of 0 is tolerated
  builder.AppendLine("#EXT-X-BITRATE:0");
  builder.AppendLine("#EXTINF:9.2,");
  builder.AppendLine("segment6.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasMediaSequenceNumber, 6);
  builder.ExpectSegment(HasUri, GURL("http://localhost/segment6.ts"));
  builder.ExpectSegment(HasBitRate, 0);

  // Large values should saturate to `DecimalInteger::max`
  builder.AppendLine("#EXT-X-BITRATE:18446744073709551");
  builder.AppendLine("#EXTINF:9.2,");
  builder.AppendLine("segment7.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasMediaSequenceNumber, 7);
  builder.ExpectSegment(HasUri, GURL("http://localhost/segment7.ts"));
  builder.ExpectSegment(HasBitRate, 18446744073709551000u);

  builder.AppendLine("#EXT-X-BITRATE:18446744073709552");
  builder.AppendLine("#EXTINF:9.2,");
  builder.AppendLine("segment8.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasMediaSequenceNumber, 8);
  builder.ExpectSegment(HasUri, GURL("http://localhost/segment8.ts"));
  builder.ExpectSegment(HasBitRate,
                        std::numeric_limits<types::DecimalInteger>::max());

  builder.AppendLine("#EXT-X-BITRATE:18446744073709551615");
  builder.AppendLine("#EXTINF:9.2,");
  builder.AppendLine("segment9.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasMediaSequenceNumber, 9);
  builder.ExpectSegment(HasUri, GURL("http://localhost/segment9.ts"));
  builder.ExpectSegment(HasBitRate,
                        std::numeric_limits<types::DecimalInteger>::max());

  builder.ExpectOk();
}

TEST(HlsMediaPlaylistTest, XByteRangeTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // EXT-X-BYTERANGE content must be a valid ByteRange
  {
    for (std::string_view x :
         {"", ":", ": 12@34", ":12@34 ", ":12@", ":12@{$offset}"}) {
      auto fork = builder;
      fork.AppendLine("#EXT-X-BYTERANGE", x);
      fork.AppendLine("#EXTINF:9.2,\t");
      fork.AppendLine("segment.ts");
      fork.ExpectError(ParseStatusCode::kMalformedTag);
    }
  }
  // EXT-X-BYTERANGE may not appear twice per-segment.
  // TODO(crbug.com/40226468): Some players support this, using only the
  // final occurrence.
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:12@34");
    fork.AppendLine("#EXT-X-BYTERANGE:34@56");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment.ts");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }
  // Offset is required if this is the first media segment.
  // TODO(crbug.com/40226468): Some players support this, default offset
  // to 0.
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:12");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment.ts");
    fork.ExpectError(ParseStatusCode::kByteRangeRequiresOffset);

    fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:12@34");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasByteRange, CreateByteRange(12, 34));
    fork.ExpectOk();
  }
  // Offset is required if the previous media segment is not a byterange.
  {
    auto fork = builder;
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment.ts");
    fork.AppendLine("#EXT-X-BYTERANGE:12");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment.ts");
    fork.ExpectError(ParseStatusCode::kByteRangeRequiresOffset);

    fork = builder;
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segment.ts"));
    fork.ExpectSegment(HasByteRange, std::nullopt);
    fork.AppendLine("#EXT-X-BYTERANGE:12@34");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segment.ts"));
    fork.ExpectSegment(HasByteRange, CreateByteRange(12, 34));
    fork.ExpectOk();
  }
  // Offset is required if the previous media segment is a byterange of a
  // different resource.
  // TODO(crbug.com/40226468): Some players support this.
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:12@34");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.AppendLine("#EXT-X-BYTERANGE:56");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment2.ts");
    fork.ExpectError(ParseStatusCode::kByteRangeRequiresOffset);

    fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:12@34");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segment1.ts"));
    fork.ExpectSegment(HasByteRange, CreateByteRange(12, 34));
    fork.AppendLine("#EXT-X-BYTERANGE:56@78");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment2.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segment2.ts"));
    fork.ExpectSegment(HasByteRange, CreateByteRange(56, 78));
    fork.ExpectOk();
  }
  // Offset is required even if a prior segment is a byterange of the same
  // resource, but not the immediately previous segment.
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:12@34");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment2.ts");
    fork.AppendLine("#EXT-X-BYTERANGE:45");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectError(ParseStatusCode::kByteRangeRequiresOffset);

    fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:12@34");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segment1.ts"));
    fork.ExpectSegment(HasByteRange, CreateByteRange(12, 34));
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment2.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segment2.ts"));
    fork.ExpectSegment(HasByteRange, std::nullopt);
    fork.AppendLine("#EXT-X-BYTERANGE:56@78");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segment1.ts"));
    fork.ExpectSegment(HasByteRange, CreateByteRange(56, 78));
    fork.ExpectOk();
  }
  // Offset can be elided if the previous segment is a byterange of the same
  // resource.
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:12@34");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segment1.ts"));
    fork.ExpectSegment(HasByteRange, CreateByteRange(12, 34));
    fork.AppendLine("#EXT-X-BYTERANGE:56");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segment1.ts"));
    fork.ExpectSegment(HasByteRange, CreateByteRange(56, 46));

    // If an explicit offset is given (even it it's eligible to be elided), it
    // must be used.
    fork.AppendLine("#EXT-X-BYTERANGE:78@99999");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasUri, GURL("http://localhost/segment1.ts"));
    fork.ExpectSegment(HasByteRange, CreateByteRange(78, 99999));
    fork.ExpectOk();
  }
  // Range given by tag may not be empty or overflow a uint64, even across
  // segments.
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:0@0");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectError(ParseStatusCode::kByteRangeInvalid);

    fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:18446744073709551615@1");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectError(ParseStatusCode::kByteRangeInvalid);

    fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:1@18446744073709551615");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectError(ParseStatusCode::kByteRangeInvalid);

    fork = builder;
    fork.AppendLine(
        "#EXT-X-BYTERANGE:18446744073709551615@18446744073709551615");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectError(ParseStatusCode::kByteRangeInvalid);

    fork = builder;
    fork.AppendLine("#EXT-X-BYTERANGE:1@18446744073709551614");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectAdditionalSegment();
    fork.ExpectSegment(HasByteRange, CreateByteRange(1, 18446744073709551614u));
    fork.ExpectOk();

    // Since the previous segment ends at uint64_t::max, an additional
    // contiguous byterange would overflow.
    fork.AppendLine("#EXT-X-BYTERANGE:1");
    fork.AppendLine("#EXTINF:9.2,\t");
    fork.AppendLine("segment1.ts");
    fork.ExpectError(ParseStatusCode::kByteRangeInvalid);
  }
}

TEST(HlsMediaPlaylistTest, XDiscontinuityTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(10));

  // Default discontinuity state is false
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);
  builder.ExpectSegment(HasDiscontinuitySequenceNumber, 0);

  builder.AppendLine("#EXT-X-DISCONTINUITY");
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, true);
  builder.ExpectSegment(HasDiscontinuitySequenceNumber, 1);

  // The discontinuity tag does not apply to subsequent segments
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, false);
  builder.ExpectSegment(HasDiscontinuitySequenceNumber, 1);

  // The discontinuity tag may appear multiple times per segment
  builder.AppendLine("#EXT-X-DISCONTINUITY");
  builder.AppendLine("#EXT-X-DISCONTINUITY");
  builder.AppendLine("#EXTINF:9.9,\t");
  builder.AppendLine("video.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDiscontinuity, true);
  builder.ExpectSegment(HasDiscontinuitySequenceNumber, 3);

  builder.ExpectOk();
}

TEST(HlsMediaPlaylistTest, XDiscontinuitySequenceTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // The EXT-X-DISCONTINUITY-SEQUENCE tag must be a valid DecimalInteger
  {
    for (const std::string_view x : {"", ":-1", ":{$foo}", ":1.5", ":one"}) {
      auto fork = builder;
      fork.AppendLine("#EXT-X-DISCONTINUITY-SEQUENCE", x);
      fork.ExpectError(ParseStatusCode::kMalformedTag);
    }
  }
  // The EXT-X-DISCONTINUITY-SEQUENCE tag may not appear twice
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-DISCONTINUITY-SEQUENCE:1");
    fork.AppendLine("#EXT-X-DISCONTINUITY-SEQUENCE:1");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-DISCONTINUITY-SEQUENCE:0");
    fork.AppendLine("#EXT-X-DISCONTINUITY");
    fork.AppendLine("#EXT-X-DISCONTINUITY-SEQUENCE:1");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }
  // The EXT-X-DISCONTINUITY-SEQUENCE tag must appear before any media segment
  {
    auto fork = builder;
    fork.AppendLine("#EXTINF:9.8,\t");
    fork.AppendLine("segment0.ts");
    fork.AppendLine("#EXT-X-DISCONTINUITY-SEQUENCE:0");
    fork.ExpectError(
        ParseStatusCode::kMediaSegmentBeforeDiscontinuitySequenceTag);
  }
  // The EXT-X-DISCONTINUITY-SEQUENCE tag must appear before any
  // EXT-X-DISCONTINUITY tag
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-DISCONTINUITY");
    fork.AppendLine("#EXT-X-DISCONTINUITY-SEQUENCE:0");
    fork.AppendLine("#EXTINF:9.8,\t");
    fork.AppendLine("segment0.ts");
    fork.ExpectError(
        ParseStatusCode::kDiscontinuityTagBeforeDiscontinuitySequenceTag);
  }

  const auto fill_playlist = [](auto& builder, auto first_media_sequence_number,
                                auto first_discontinuity_sequence_number) {
    builder.AppendLine("#EXTINF:9.8,\t");
    builder.AppendLine("segment0.ts");
    builder.ExpectAdditionalSegment();
    builder.ExpectSegment(HasUri, GURL("http://localhost/segment0.ts"));
    builder.ExpectSegment(HasDiscontinuity, false);
    builder.ExpectSegment(HasMediaSequenceNumber, first_media_sequence_number);
    builder.ExpectSegment(HasDiscontinuitySequenceNumber,
                          first_discontinuity_sequence_number);

    builder.AppendLine("#EXT-X-DISCONTINUITY");
    builder.AppendLine("#EXTINF:9.8,\t");
    builder.AppendLine("segment1.ts");
    builder.ExpectAdditionalSegment();
    builder.ExpectSegment(HasDiscontinuity, true);
    builder.ExpectSegment(HasMediaSequenceNumber,
                          first_media_sequence_number + 1);
    builder.ExpectSegment(HasDiscontinuitySequenceNumber,
                          first_discontinuity_sequence_number + 1);

    builder.AppendLine("#EXTINF:9.8,\t");
    builder.AppendLine("segment2.ts");
    builder.ExpectAdditionalSegment();
    builder.ExpectSegment(HasDiscontinuity, false);
    builder.ExpectSegment(HasMediaSequenceNumber,
                          first_media_sequence_number + 2);
    builder.ExpectSegment(HasDiscontinuitySequenceNumber,
                          first_discontinuity_sequence_number + 1);
  };

  // If the playlist does not contain the EXT-X-DISCONTINUITY-SEQUENCE tag, the
  // default starting value is 0.
  auto fork = builder;
  fill_playlist(fork, 0, 0);
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:10");
  fill_playlist(fork, 10, 0);
  fork.ExpectOk();

  // If the playlist has the EXT-X-DISCONTINUITY-SEQUENCE tag, it specifies the
  // starting value.
  fork = builder;
  fork.AppendLine("#EXT-X-DISCONTINUITY-SEQUENCE:5");
  fill_playlist(fork, 0, 5);
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:10");
  fork.AppendLine("#EXT-X-DISCONTINUITY-SEQUENCE:5");
  fill_playlist(fork, 10, 5);
  fork.ExpectOk();

  // If the very first segment is a discontinuity, it should still have a
  // subsequent discontinuity sequence number.
  fork = builder;
  fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:10");
  fork.AppendLine("#EXT-X-DISCONTINUITY");
  fork.AppendLine("#EXTINF:9.2,\t");
  fork.AppendLine("segment.ts");
  fork.ExpectAdditionalSegment();
  fork.ExpectSegment(HasDiscontinuity, true);
  fork.ExpectSegment(HasMediaSequenceNumber, 10);
  fork.ExpectSegment(HasDiscontinuitySequenceNumber, 1);
  fill_playlist(fork, 11, 1);
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:10");
  fork.AppendLine("#EXT-X-DISCONTINUITY-SEQUENCE:5");
  fork.AppendLine("#EXT-X-DISCONTINUITY");
  fork.AppendLine("#EXTINF:9.2,\t");
  fork.AppendLine("segment.ts");
  fork.ExpectAdditionalSegment();
  fork.ExpectSegment(HasDiscontinuity, true);
  fork.ExpectSegment(HasMediaSequenceNumber, 10);
  fork.ExpectSegment(HasDiscontinuitySequenceNumber, 6);
  fill_playlist(fork, 11, 6);
  fork.ExpectOk();
}

TEST(HlsMediaPlaylistTest, XEndListTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // Without the 'EXT-X-ENDLIST' tag, the default value is false, regardless of
  // the playlist type.
  {
    for (const std::string_view type : {"", "EVENT", "VOD"}) {
      auto fork = builder;
      if (!type.empty()) {
        fork.AppendLine("#EXT-X-PLAYLIST-TYPE:", type);
      }
      fork.ExpectPlaylist(IsEndList, false);
      fork.ExpectOk();
    }
  }

  // The 'EXT-X-ENDLIST' tag may not have any content
  {
    for (const std::string_view x : {"", "FOO=BAR", "1"}) {
      auto fork = builder;
      fork.AppendLine("#EXT-X-ENDLIST:", x);
      fork.ExpectError(ParseStatusCode::kMalformedTag);
    }
  }

  // The EXT-X-ENDLIST tag can appear anywhere in the playlist
  builder.AppendLine("#EXTINF:9.2,\t");
  builder.AppendLine("segment0.ts");
  builder.ExpectAdditionalSegment();

  builder.AppendLine("#EXT-X-ENDLIST");
  builder.ExpectPlaylist(IsEndList, true);

  builder.AppendLine("#EXTINF:9.2,\n");
  builder.AppendLine("segment1.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectOk();

  // The EXT-X-ENDLIST tag may not appear twice
  builder.AppendLine("#EXT-X-ENDLIST");
  builder.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
}

TEST(HlsMediaPlaylistTest, XGapTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(10));

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

TEST(HlsMediaPlaylistTest, XIFramesOnlyTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // Without the 'EXT-X-I-FRAMES-ONLY' tag, the default value is false.
  {
    auto fork = builder;
    fork.ExpectPlaylist(IsIFramesOnly, false);
    fork.ExpectOk();
  }

  // The 'EXT-X-I-FRAMES-ONLY' tag may not have any content
  {
    for (const std::string_view x : {"", "FOO=BAR", "1"}) {
      auto fork = builder;
      fork.AppendLine("#EXT-X-I-FRAMES-ONLY:", x);
      fork.ExpectError(ParseStatusCode::kMalformedTag);
    }
  }

  builder.AppendLine("#EXT-X-I-FRAMES-ONLY");
  builder.ExpectPlaylist(IsIFramesOnly, true);

  // This should not affect the calculation of the playlist's duration
  builder.AppendLine("#EXTINF:10,\t");
  builder.AppendLine("segment0.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDuration, base::Seconds(10));

  builder.AppendLine("#EXTINF:10,\t");
  builder.AppendLine("segment1.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDuration, base::Seconds(10));

  builder.ExpectPlaylist(HasComputedDuration, base::Seconds(20));
  builder.ExpectOk();

  // The 'EXT-X-I-FRAMES-ONLY' tag should not appear twice
  builder.AppendLine("#EXT-X-I-FRAMES-ONLY");
  builder.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
}

TEST(HlsMediaPlaylistTest, XMapTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // The EXT-X-MAP tag must be valid
  for (std::string_view x : {"", "BYTERANGE=\"10\"", "URI=foo.ts"}) {
    auto fork = builder;
    fork.AppendLine("#EXT-X-MAP:", x);
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }

  // The EXT-X-MAP tag only applies to subsequent elements
  builder.AppendLine("#EXTINF:9.2,\t");
  builder.AppendLine("foo1.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDuration, base::Seconds(9.2));
  builder.ExpectSegment(HasUri, GURL("http://localhost/foo1.ts"));
  builder.ExpectSegment(HasInitializationSegment, nullptr);

  builder.AppendLine("#EXT-X-MAP:URI=\"init1.ts\"");
  auto init1 = base::MakeRefCounted<MediaSegment::InitializationSegment>(
      GURL("http://localhost/init1.ts"), std::nullopt);

  builder.AppendLine("#EXTINF:9.2,\t");
  builder.AppendLine("foo2.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDuration, base::Seconds(9.2));
  builder.ExpectSegment(HasUri, GURL("http://localhost/foo2.ts"));
  builder.ExpectSegment(HasInitializationSegment, init1);

  builder.AppendLine("#EXTINF:9.2,\t");
  builder.AppendLine("foo3.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDuration, base::Seconds(9.2));
  builder.ExpectSegment(HasUri, GURL("http://localhost/foo3.ts"));
  builder.ExpectSegment(HasInitializationSegment, init1);

  // Consecutive EXT-X-MAP tags are tolerated
  builder.AppendLine("#EXT-X-MAP:URI=\"init2.ts\"");
  builder.AppendLine("#EXT-X-MAP:URI=\"init3.ts\",BYTERANGE=\"10@0\"");
  auto init3 = base::MakeRefCounted<MediaSegment::InitializationSegment>(
      GURL("http://localhost/init3.ts"),
      types::ByteRange::Validate(10, 0).value());

  builder.AppendLine("#EXTINF:9.2,\t");
  builder.AppendLine("foo4.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDuration, base::Seconds(9.2));
  builder.ExpectSegment(HasUri, GURL("http://localhost/foo4.ts"));
  builder.ExpectSegment(HasInitializationSegment, init3);

  // If the BYTERANGE offset is not specified, it defaults to 0 (even if the
  // previous, initialization segment is a byterange of the same resource)
  builder.AppendLine("#EXT-X-MAP:URI=\"init3.ts\",BYTERANGE=\"10\"");

  builder.AppendLine("#EXTINF:9.2,\t");
  builder.AppendLine("foo5.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasDuration, base::Seconds(9.2));
  builder.ExpectSegment(HasUri, GURL("http://localhost/foo5.ts"));
  builder.ExpectSegment(HasInitializationSegment, init3);

  builder.ExpectOk();
}

TEST(HlsMediaPlaylistTest, XMediaSequenceTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // The EXT-X-MEDIA-SEQUENCE tag's content must be a valid DecimalInteger
  {
    for (const std::string_view x : {"", ":-1", ":{$foo}", ":1.5", ":one"}) {
      auto fork = builder;
      fork.AppendLine("#EXT-X-MEDIA-SEQUENCE", x);
      fork.ExpectError(ParseStatusCode::kMalformedTag);
    }
  }
  // The EXT-X-MEDIA-SEQUENCE tag may not appear twice
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:0");
    fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:1");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }
  // The EXT-X-MEDIA-SEQUENCE tag must appear before any media segment
  {
    auto fork = builder;
    fork.AppendLine("#EXTINF:9.8,\t");
    fork.AppendLine("segment0.ts");
    fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:0");
    fork.ExpectError(ParseStatusCode::kMediaSegmentBeforeMediaSequenceTag);
  }

  const auto fill_playlist = [](auto& builder, auto first_sequence_number) {
    builder.AppendLine("#EXTINF:9.8,\t");
    builder.AppendLine("segment0.ts");
    builder.ExpectAdditionalSegment();
    builder.ExpectSegment(HasUri, GURL("http://localhost/segment0.ts"));
    builder.ExpectSegment(HasMediaSequenceNumber, first_sequence_number);
    builder.ExpectSegment(HasDiscontinuitySequenceNumber, 0);

    builder.AppendLine("#EXTINF:9.8,\t");
    builder.AppendLine("segment1.ts");
    builder.ExpectAdditionalSegment();
    builder.ExpectSegment(HasMediaSequenceNumber, first_sequence_number + 1);
    builder.ExpectSegment(HasDiscontinuitySequenceNumber, 0);

    builder.AppendLine("#EXTINF:9.8,\t");
    builder.AppendLine("segment2.ts");
    builder.ExpectAdditionalSegment();
    builder.ExpectSegment(HasMediaSequenceNumber, first_sequence_number + 2);
    builder.ExpectSegment(HasDiscontinuitySequenceNumber, 0);
  };

  // If the playlist does not contain the EXT-X-MEDIA-SEQUENCE tag, the default
  // starting segment number is 0.
  auto fork = builder;
  fill_playlist(fork, 0);
  fork.ExpectPlaylist(HasMediaSequenceTag, false);
  fork.ExpectOk();

  // If the playlist has the EXT-X-MEDIA-SEQUENCE tag, it specifies the starting
  // segment number.
  fork = builder;
  fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:0");
  fill_playlist(fork, 0);
  fork.ExpectPlaylist(HasMediaSequenceTag, true);
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:15");
  fill_playlist(fork, 15);
  fork.ExpectPlaylist(HasMediaSequenceTag, true);
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-MEDIA-SEQUENCE:9999");
  fill_playlist(fork, 9999);
  fork.ExpectPlaylist(HasMediaSequenceTag, true);
  fork.ExpectOk();
}

TEST(HlsMediaPlaylistTest, XPartInfTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:100");
  builder.AppendLine("#EXT-X-SERVER-CONTROL:PART-HOLD-BACK=500");

  // EXT-X-PART-INF tag must be well-formed
  for (std::string_view x : {"", ":", ":TARGET=1", ":PART-TARGET=two"}) {
    auto fork = builder;
    fork.AppendLine("#EXT-X-PART-INF", x);
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }

  auto fork = builder;
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=0");
  fork.ExpectPlaylist(
      HasPartialSegmentInfo,
      MediaPlaylist::PartialSegmentInfo{.target_duration = base::Seconds(0)});
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=1");
  fork.ExpectPlaylist(
      HasPartialSegmentInfo,
      MediaPlaylist::PartialSegmentInfo{.target_duration = base::Seconds(1)});
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=1.2");
  fork.ExpectPlaylist(
      HasPartialSegmentInfo,
      MediaPlaylist::PartialSegmentInfo{.target_duration = base::Seconds(1.2)});
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=99.99");
  fork.ExpectPlaylist(HasPartialSegmentInfo,
                      MediaPlaylist::PartialSegmentInfo{
                          .target_duration = base::Seconds(99.99)});
  fork.ExpectOk();

  // PART-TARGET may not exceed the playlist's target duration
  fork = builder;
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=100");
  fork.ExpectPlaylist(HasTargetDuration, base::Seconds(100));
  fork.ExpectPlaylist(
      HasPartialSegmentInfo,
      MediaPlaylist::PartialSegmentInfo{.target_duration = base::Seconds(100)});
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=101");
  fork.ExpectError(ParseStatusCode::kPartTargetDurationExceedsTargetDuration);

  // The EXT-X-PART-INF tag may not appear twice
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=10");
  fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
}

TEST(HlsMediaPlaylistTest, XPlaylistTypeTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // Without the EXT-X-PLAYLIST-TYPE tag, the playlist has no type.
  {
    auto fork = builder;
    fork.ExpectPlaylist(HasType, std::nullopt);
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

TEST(HlsMediaPlaylistTest, XServerControlTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:6");
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(6));

  // Without the EXT-X-SERVER-CONTROL tag, certain properties have default
  // values
  auto fork = builder;
  fork.ExpectPlaylist(HasSkipBoundary, std::nullopt);
  fork.ExpectPlaylist(CanSkipDateRanges, false);
  fork.ExpectPlaylist(HasHoldBackDistance, base::Seconds(6) * 3);
  fork.ExpectPlaylist(HasPartHoldBackDistance, std::nullopt);
  fork.ExpectPlaylist(CanBlockReload, false);
  fork.ExpectOk();
  // An empty EXT-X-SERVER-CONTROL tag shouldn't change these defaults
  fork.AppendLine("#EXT-X-SERVER-CONTROL:");
  fork.ExpectOk();

  // This tag may not appear twice
  fork.AppendLine("#EXT-X-SERVER-CONTROL:");
  fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);

  // If attributes are malformed, playlist should be rejected
  fork = builder;
  fork.AppendLine("#EXT-X-SERVER-CONTROL:CAN-SKIP-UNTIL={$foo}");
  fork.ExpectError(ParseStatusCode::kMalformedTag);

  // The CAN-SKIP-UNTIL attribute must be at least six times the target duration
  fork = builder;
  fork.AppendLine("#EXT-X-SERVER-CONTROL:CAN-SKIP-UNTIL=35");
  fork.ExpectError(ParseStatusCode::kSkipBoundaryTooLow);

  fork = builder;
  fork.AppendLine("#EXT-X-SERVER-CONTROL:CAN-SKIP-UNTIL=36");
  fork.ExpectPlaylist(HasSkipBoundary, base::Seconds(36));
  fork.ExpectPlaylist(CanSkipDateRanges, false);
  fork.ExpectPlaylist(HasHoldBackDistance, base::Seconds(6) * 3);
  fork.ExpectPlaylist(HasPartHoldBackDistance, std::nullopt);
  fork.ExpectPlaylist(CanBlockReload, false);
  fork.ExpectOk();

  // The CAN-SKIP-DATERANGES tag may not appear without CAN-SKIP-UNTIL
  fork = builder;
  fork.AppendLine("#EXT-X-SERVER-CONTROL:CAN-SKIP-DATERANGES=YES");
  fork.ExpectError(ParseStatusCode::kMalformedTag);

  fork = builder;
  fork.AppendLine(
      "#EXT-X-SERVER-CONTROL:CAN-SKIP-DATERANGES=YES,CAN-SKIP-UNTIL=40");
  fork.ExpectPlaylist(CanSkipDateRanges, true);
  fork.ExpectPlaylist(HasSkipBoundary, base::Seconds(40));
  fork.ExpectPlaylist(HasHoldBackDistance, base::Seconds(6) * 3);
  fork.ExpectPlaylist(HasPartHoldBackDistance, std::nullopt);
  fork.ExpectPlaylist(CanBlockReload, false);
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine(
      "#EXT-X-SERVER-CONTROL:CAN-SKIP-UNTIL=40,CAN-SKIP-DATERANGES=YES");
  fork.ExpectPlaylist(CanSkipDateRanges, true);
  fork.ExpectPlaylist(HasSkipBoundary, base::Seconds(40));
  fork.ExpectPlaylist(HasHoldBackDistance, base::Seconds(6) * 3);
  fork.ExpectPlaylist(HasPartHoldBackDistance, std::nullopt);
  fork.ExpectPlaylist(CanBlockReload, false);
  fork.ExpectOk();

  // The 'HOLD-BACK' attribute must be at least three times the playlist's
  // target duration
  fork = builder;
  fork.AppendLine("#EXT-X-SERVER-CONTROL:HOLD-BACK=18");
  fork.ExpectPlaylist(HasHoldBackDistance, base::Seconds(18));
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-SERVER-CONTROL:HOLD-BACK=17");
  fork.ExpectError(ParseStatusCode::kHoldBackDistanceTooLow);

  fork = builder;
  fork.AppendLine("#EXT-X-SERVER-CONTROL:HOLD-BACK=17.999");
  fork.ExpectError(ParseStatusCode::kHoldBackDistanceTooLow);

  // The 'EXT-X-PART-INF' tag requires the 'PART-HOLD-BACK' field
  fork = builder;
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=0.2");
  fork.ExpectError(ParseStatusCode::kPartInfTagWithoutPartHoldBack);

  fork = builder;
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=0.2");
  fork.AppendLine("#EXT-X-SERVER-CONTROL:");
  fork.ExpectError(ParseStatusCode::kPartInfTagWithoutPartHoldBack);

  fork = builder;
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=0.2");
  fork.AppendLine("#EXT-X-SERVER-CONTROL:PART-HOLD-BACK=0.5");
  fork.ExpectPlaylist(
      HasPartialSegmentInfo,
      MediaPlaylist::PartialSegmentInfo{.target_duration = base::Seconds(0.2)});
  fork.ExpectPlaylist(HasPartHoldBackDistance, base::Seconds(0.5));
  fork.ExpectPlaylist(HasSkipBoundary, std::nullopt);
  fork.ExpectPlaylist(CanSkipDateRanges, false);
  fork.ExpectPlaylist(HasHoldBackDistance, base::Seconds(6) * 3);
  fork.ExpectPlaylist(CanBlockReload, false);
  fork.ExpectOk();

  // PART-HOLD-BACK must not be less than PART-TARGET * 2 (unless that tag
  // doesn't exist)
  fork = builder;
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=0.2");
  fork.AppendLine("#EXT-X-SERVER-CONTROL:PART-HOLD-BACK=0.4");
  fork.ExpectPlaylist(
      HasPartialSegmentInfo,
      MediaPlaylist::PartialSegmentInfo{.target_duration = base::Seconds(0.2)});
  fork.ExpectPlaylist(HasPartHoldBackDistance, base::Seconds(0.4));
  fork.ExpectPlaylist(HasSkipBoundary, std::nullopt);
  fork.ExpectPlaylist(CanSkipDateRanges, false);
  fork.ExpectPlaylist(HasHoldBackDistance, base::Seconds(6) * 3);
  fork.ExpectPlaylist(CanBlockReload, false);
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine("#EXT-X-PART-INF:PART-TARGET=0.2");
  fork.AppendLine("#EXT-X-SERVER-CONTROL:PART-HOLD-BACK=0.3");
  fork.ExpectError(ParseStatusCode::kPartHoldBackDistanceTooLow);

  fork = builder;
  fork.AppendLine("#EXT-X-SERVER-CONTROL:PART-HOLD-BACK=0.3");
  fork.ExpectPlaylist(HasPartialSegmentInfo, std::nullopt);
  fork.ExpectPlaylist(HasPartHoldBackDistance, base::Seconds(0.3));
  fork.ExpectPlaylist(HasSkipBoundary, std::nullopt);
  fork.ExpectPlaylist(CanSkipDateRanges, false);
  fork.ExpectPlaylist(HasHoldBackDistance, base::Seconds(6) * 3);
  fork.ExpectPlaylist(CanBlockReload, false);
  fork.ExpectOk();

  // Test the effect of the 'CAN-BLOCK-RELOAD' attribute
  fork = builder;
  fork.AppendLine("#EXT-X-SERVER-CONTROL:CAN-BLOCK-RELOAD=YES");
  fork.ExpectPlaylist(CanBlockReload, true);
  fork.ExpectPlaylist(HasPartialSegmentInfo, std::nullopt);
  fork.ExpectPlaylist(HasPartHoldBackDistance, std::nullopt);
  fork.ExpectPlaylist(HasSkipBoundary, std::nullopt);
  fork.ExpectPlaylist(CanSkipDateRanges, false);
  fork.ExpectPlaylist(HasHoldBackDistance, base::Seconds(6) * 3);
  fork.ExpectOk();
}

TEST(HlsMediaPlaylistTest, XSkipTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-TARGETDURATION:10");

  // The XSkip tag may not appear unless a playlist delta update was requested.
  builder.AppendLine("#EXT-X-SKIP:SKIPPED-SEGMENTS=10");
  builder.ExpectError(ParseStatusCode::kPlaylistHasUnexpectedDeltaUpdate);
}

TEST(HlsMediaPlaylistTest, XTargetDurationTag) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");

  // The XTargetDurationTag tag is required
  builder.ExpectError(ParseStatusCode::kMediaPlaylistMissingTargetDuration);

  // The XTargetDurationTag must appear exactly once
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(10));
  builder.ExpectOk();

  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-TARGETDURATION:10");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-TARGETDURATION:11");
    fork.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
  }

  // The XTargetDurationTag must be a valid DecimalInteger (unsigned)
  for (std::string_view x : {"-1", "0.5", "-1.5", "999999999999999999999"}) {
    MediaPlaylistTestBuilder builder2;
    builder2.AppendLine("#EXTM3U");
    builder2.AppendLine("#EXT-X-TARGETDURATION:", x);
    builder2.ExpectError(ParseStatusCode::kMalformedTag);
  }

  // The target duration value may not exceed this implementation's max
  builder = MediaPlaylistTestBuilder();
  builder.AppendLine("#EXTM3U");
  builder.AppendLine(
      "#EXT-X-TARGETDURATION:",
      base::NumberToString(MediaPlaylist::kMaxTargetDuration.InSeconds()));
  builder.ExpectPlaylist(
      HasTargetDuration,
      base::Seconds(MediaPlaylist::kMaxTargetDuration.InSeconds()));
  builder.ExpectOk();

  builder = MediaPlaylistTestBuilder();
  builder.AppendLine("#EXTM3U");
  builder.AppendLine(
      "#EXT-X-TARGETDURATION:",
      base::NumberToString(MediaPlaylist::kMaxTargetDuration.InSeconds() + 1));
  builder.ExpectError(ParseStatusCode::kTargetDurationExceedsMax);
}

TEST(HlsMediaPlaylistTest, XKeyTagAppliesToSegments) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");

  // The XTargetDurationTag tag is required
  builder.ExpectError(ParseStatusCode::kMediaPlaylistMissingTargetDuration);

  // The XTargetDurationTag must appear exactly once
  builder.AppendLine("#EXT-X-TARGETDURATION:2");
  builder.ExpectPlaylist(HasTargetDuration, base::Seconds(2));
  builder.ExpectOk();

  builder.AppendLine("#EXT-X-MEDIA-SEQUENCE:0");
  builder.ExpectOk();

  builder.AppendLine("#EXT-X-PLAYLIST-TYPE:VOD");
  builder.ExpectOk();

  builder.AppendLine("#EXTINF:1.600000,");
  builder.AppendLine("data00.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasUri, GURL("http://localhost/data00.ts"));
  builder.ExpectSegment(HasMediaSequenceNumber, 0);
  builder.ExpectSegment(HasEncryptionData, std::nullopt);
  builder.ExpectOk();

  builder.AppendLine(
      "#EXT-X-KEY:METHOD=AES-128,URI=\"enc.key\",IV="
      "0x00000000000000000000000000000042");
  builder.AppendLine("#EXTINF:1.60000,");
  builder.AppendLine("data01.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasUri, GURL("http://localhost/data01.ts"));
  builder.ExpectSegment(HasMediaSequenceNumber, 1);
  builder.ExpectSegment(
      HasEncryptionData,
      std::make_tuple(GURL("http://localhost/enc.key"), XKeyTagMethod::kAES128,
                      XKeyTagKeyFormat::kIdentity, std::make_tuple(0, 0x42)));
  builder.ExpectOk();

  builder.AppendLine("#EXT-X-KEY:METHOD=NONE");
  builder.AppendLine("#EXTINF:1.60000,");
  builder.AppendLine("data02.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasUri, GURL("http://localhost/data02.ts"));
  builder.ExpectSegment(HasMediaSequenceNumber, 2);
  builder.ExpectSegment(HasEncryptionData, std::nullopt);
  builder.ExpectOk();

  builder.AppendLine(
      "#EXT-X-KEY:METHOD=AES-128,URI=\"enc.key\",KEYFORMAT=\"identity\"");
  builder.AppendLine("#EXTINF:1.60000,");
  builder.AppendLine("data03.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasUri, GURL("http://localhost/data03.ts"));
  builder.ExpectSegment(HasMediaSequenceNumber, 3);
  builder.ExpectSegment(
      HasEncryptionData,
      std::make_tuple(GURL("http://localhost/enc.key"), XKeyTagMethod::kAES128,
                      XKeyTagKeyFormat::kIdentity, std::make_tuple(0, 3)));
  builder.ExpectOk();

  builder.AppendLine("#EXTINF:1.600000,");
  builder.AppendLine("data04.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasUri, GURL("http://localhost/data04.ts"));
  builder.ExpectSegment(HasMediaSequenceNumber, 4);
  builder.ExpectSegment(
      HasEncryptionData,
      std::make_tuple(GURL("http://localhost/enc.key"), XKeyTagMethod::kAES128,
                      XKeyTagKeyFormat::kIdentity, std::make_tuple(0, 4)));
  builder.ExpectOk();

  builder.AppendLine(
      "#EXT-X-KEY:METHOD=SAMPLE-AES,URI=\"enc.key\",KEYFORMAT=\"identity\"");
  builder.AppendLine("#EXTINF:1.60000,");
  builder.AppendLine("data05.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasUri, GURL("http://localhost/data05.ts"));
  builder.ExpectSegment(HasMediaSequenceNumber, 5);
  builder.ExpectSegment(
      HasEncryptionData,
      std::make_tuple(GURL("http://localhost/enc.key"),
                      XKeyTagMethod::kSampleAES, XKeyTagKeyFormat::kIdentity,
                      std::make_tuple(0, 5)));
  builder.ExpectOk();

  builder.AppendLine(
      "#EXT-X-KEY:METHOD=SAMPLE-AES-CTR,URI=\"enc.key\",KEYFORMAT="
      "\"identity\"");
  builder.AppendLine("#EXTINF:1.60000,");
  builder.AppendLine("data06.ts");
  builder.ExpectAdditionalSegment();
  builder.ExpectSegment(HasUri, GURL("http://localhost/data06.ts"));
  builder.ExpectSegment(HasMediaSequenceNumber, 6);
  builder.ExpectSegment(
      HasEncryptionData,
      std::make_tuple(GURL("http://localhost/enc.key"),
                      XKeyTagMethod::kSampleAESCTR, XKeyTagKeyFormat::kIdentity,
                      std::make_tuple(0, 6)));
  builder.ExpectOk();
}

}  // namespace media::hls
