// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/multivariant_playlist.h"

#include <optional>

#include "media/formats/hls/multivariant_playlist_test_builder.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

// This test is similar to the `HlsMediaPlaylistTest` test of the same name, but
// due to subtle differences between media playlists and multivariant playlists
// its difficult to combine them. If new cases are added here that are also
// relevant to media playlists, they should be added to that test as well.
TEST(HlsMultivariantPlaylistTest, VariableSubstitution) {
  MultivariantPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXT-X-VERSION:8");
  builder.SetVersion(8);

  builder.AppendLine(
      R"(#EXT-X-DEFINE:NAME="HOST",VALUE="http://www.example.com")");
  builder.AppendLine(
      R"(#EXT-X-DEFINE:NAME="CODECS",VALUE="mp4a.40.2,avc1.4d401e")");

  // Valid variable references within URIs or quoted-string values may be
  // substituted
  builder.AppendLine(R"(#EXT-X-STREAM-INF:BANDWIDTH=100,CODECS="{$CODECS}")");
  builder.AppendLine("{$HOST}/playlist1.m3u8");
  builder.ExpectAdditionalVariant();
  builder.ExpectVariant(HasPrimaryRenditionUri,
                        GURL("http://www.example.com/playlist1.m3u8"));
  builder.ExpectVariant(HasCodecs,
                        std::vector<std::string>{"mp4a.40.2", "avc1.4d401e"});

  // Invalid variable references should result in an error
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=101");
    fork.AppendLine("{$HOST}/{$movie}/playlist2.m3u8");
    fork.ExpectError(ParseStatusCode::kVariableUndefined);
  }
  {
    auto fork = builder;
    fork.AppendLine(R"(#EXT-X-STREAM-INF:BANDWIDTH=101,CODECS="{$CODEX}")");
    fork.AppendLine("{$HOST}/playlist2.m3u8");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }

  // Variable references outside of valid substitution points should not be
  // substituted
  {
    auto fork = builder;
    fork.AppendLine(R"(#EXT-X-DEFINE:NAME="BW",VALUE="102")");
    fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH={$BW}");
    fork.AppendLine("playlist3.m3u8");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }
  {
    auto fork = builder;
    fork.AppendLine(R"(#EXT-X-DEFINE:NAME="AVG-BW",VALUE="102")");
    fork.AppendLine(
        "#EXT-X-STREAM-INF:BANDWIDTH=100,AVERAGE-BANDWIDTH={$AVG-BW}");
    fork.AppendLine("playlist3.m3u8");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }
  {
    auto fork = builder;
    fork.AppendLine(R"(#EXT-X-DEFINE:NAME="SCORE",VALUE="10")");
    fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=100,SCORE={$SCORE}");
    fork.AppendLine("playlist3.m3u8");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }
  {
    auto fork = builder;
    fork.AppendLine(R"(#EXT-X-DEFINE:NAME="RES",VALUE="1920x1080")");
    fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=100,RESOLUTION={$RES}");
    fork.AppendLine("playlist3.m3u8");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }
  {
    auto fork = builder;
    fork.AppendLine(R"(#EXT-X-DEFINE:NAME="FR",VALUE="30")");
    fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=100,FRAME-RATE={$FR}");
    fork.AppendLine("playlist3.m3u8");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }

  // Redefinition is not allowed
  {
    auto fork = builder;
    fork.AppendLine(
        R"(#EXT-X-DEFINE:NAME="HOST",VALUE="https://www.google.com")");
    fork.ExpectError(ParseStatusCode::kVariableDefinedMultipleTimes);
  }

  // Importing in a parentless playlist is not allowed
  {
    auto fork = builder;
    fork.AppendLine(R"(#EXT-X-DEFINE:IMPORT="IMPORTED")");
    fork.ExpectError(ParseStatusCode::kImportedVariableInParentlessPlaylist);
  }

  // Variables are not resolved recursively
  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="BAR",VALUE="BAZ")");
  builder.AppendLine(R"(#EXT-X-DEFINE:NAME="FOO",VALUE="{$BAR}")");
  builder.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=101");
  builder.AppendLine("http://{$FOO}.com/playlist4.m3u8");
  builder.ExpectAdditionalVariant();
  builder.ExpectVariant(HasPrimaryRenditionUri,
                        GURL("http://{$BAR}.com/playlist4.m3u8"));

  builder.ExpectOk();
}

TEST(HlsMultivariantPlaylistTest, MediaPlaylistTag) {
  MultivariantPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");

  // Multivariant playlists may not contain tags exclusive to media playlists
  for (TagName name = ToTagName(MediaPlaylistTagName::kMinValue);
       name <= ToTagName(MediaPlaylistTagName::kMaxValue); ++name) {
    auto tag_line = "#" + std::string{TagNameToString(name)};
    auto fork = builder;
    fork.AppendLine(tag_line);
    fork.ExpectError(ParseStatusCode::kMultivariantPlaylistHasMediaPlaylistTag);
  }
}

TEST(HlsMultivariantPlaylistTest, XStreamInfTag) {
  MultivariantPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");

  // 'BANDWIDTH' attribute is required
  builder.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=100");
  builder.AppendLine("playlist1.m3u8");
  builder.ExpectAdditionalVariant();
  builder.ExpectVariant(HasPrimaryRenditionUri,
                        GURL("http://localhost/playlist1.m3u8"));
  builder.ExpectVariant(HasBandwidth, 100);
  builder.ExpectVariant(HasAverageBandwidth, std::nullopt);
  builder.ExpectVariant(HasScore, std::nullopt);
  builder.ExpectVariant(HasCodecs, std::nullopt);
  builder.ExpectVariant(HasResolution, std::nullopt);
  builder.ExpectVariant(HasFrameRate, std::nullopt);
  builder.ExpectOk();

  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=101");
    fork.ExpectError(ParseStatusCode::kMalformedTag);
  }

  // EXT-X-STREAM-INF tags that are not immediately followed by URIs are
  // invalid.
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=101");
    fork.ExpectError(ParseStatusCode::kXStreamInfTagNotFollowedByUri);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=101");
    fork.AppendLine("#EXTM3U");
    fork.AppendLine("playlist2.m3u8");
    fork.ExpectError(ParseStatusCode::kXStreamInfTagNotFollowedByUri);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=102");
    fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=103");
    fork.AppendLine("playlist3.m3u8");
    fork.ExpectError(ParseStatusCode::kXStreamInfTagNotFollowedByUri);
  }
  {
    auto fork = builder;
    fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=104");
    fork.AppendLine("#EXT-X-FOO-BAR");
    fork.AppendLine("playlist4.m3u8");
    fork.ExpectError(ParseStatusCode::kXStreamInfTagNotFollowedByUri);
  }

  // Blank lines are tolerated
  builder.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=105");
  builder.AppendLine("");
  builder.AppendLine("playlist4.m3u8");
  builder.ExpectAdditionalVariant();
  builder.ExpectVariant(HasPrimaryRenditionUri,
                        GURL("http://localhost/playlist4.m3u8"));
  builder.ExpectVariant(HasBandwidth, 105);
  builder.ExpectVariant(HasAverageBandwidth, std::nullopt);
  builder.ExpectVariant(HasScore, std::nullopt);
  builder.ExpectVariant(HasCodecs, std::nullopt);
  builder.ExpectVariant(HasResolution, std::nullopt);
  builder.ExpectVariant(HasFrameRate, std::nullopt);
  builder.ExpectOk();

  // URIs without corresponding EXT-X-STREAM-INF tags are not allowed
  {
    auto fork = builder;
    fork.AppendLine("playlist5.m3u8");
    fork.ExpectError(ParseStatusCode::kVariantMissingStreamInfTag);
  }

  // Check the value of the 'AVERAGE-BANDWIDTH' attribute
  builder.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=106,AVERAGE-BANDWIDTH=105");
  builder.AppendLine("playlist5.m3u8");
  builder.ExpectAdditionalVariant();
  builder.ExpectVariant(HasPrimaryRenditionUri,
                        GURL("http://localhost/playlist5.m3u8"));
  builder.ExpectVariant(HasBandwidth, 106u);
  builder.ExpectVariant(HasAverageBandwidth, 105u);
  builder.ExpectVariant(HasScore, std::nullopt);
  builder.ExpectVariant(HasCodecs, std::nullopt);
  builder.ExpectVariant(HasResolution, std::nullopt);
  builder.ExpectVariant(HasFrameRate, std::nullopt);
  builder.ExpectOk();

  // Check the value of the 'SCORE' attribute
  builder.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=107,SCORE=10.5");
  builder.AppendLine("playlist6.m3u8");
  builder.ExpectAdditionalVariant();
  builder.ExpectVariant(HasPrimaryRenditionUri,
                        GURL("http://localhost/playlist6.m3u8"));
  builder.ExpectVariant(HasBandwidth, 107u);
  builder.ExpectVariant(HasAverageBandwidth, std::nullopt);
  builder.ExpectVariant(HasScore, 10.5);
  builder.ExpectVariant(HasCodecs, std::nullopt);
  builder.ExpectVariant(HasResolution, std::nullopt);
  builder.ExpectVariant(HasFrameRate, std::nullopt);
  builder.ExpectOk();

  // Check the value of the 'CODECS' attribute
  builder.AppendLine(R"(#EXT-X-STREAM-INF:BANDWIDTH=108,CODECS="foo,bar")");
  builder.AppendLine("playlist7.m3u8");
  builder.ExpectAdditionalVariant();
  builder.ExpectVariant(HasPrimaryRenditionUri,
                        GURL("http://localhost/playlist7.m3u8"));
  builder.ExpectVariant(HasBandwidth, 108u);
  builder.ExpectVariant(HasAverageBandwidth, std::nullopt);
  builder.ExpectVariant(HasScore, std::nullopt);
  builder.ExpectVariant(HasCodecs, std::vector<std::string>{"foo", "bar"});
  builder.ExpectVariant(HasResolution, std::nullopt);
  builder.ExpectVariant(HasFrameRate, std::nullopt);
  builder.ExpectOk();

  // Check the value of the 'RESOLUTION' attribute
  builder.AppendLine(R"(#EXT-X-STREAM-INF:BANDWIDTH=109,RESOLUTION=1920x1080)");
  builder.AppendLine("playlist8.m3u8");
  builder.ExpectAdditionalVariant();
  builder.ExpectVariant(HasPrimaryRenditionUri,
                        GURL("http://localhost/playlist8.m3u8"));
  builder.ExpectVariant(HasBandwidth, 109u);
  builder.ExpectVariant(HasAverageBandwidth, std::nullopt);
  builder.ExpectVariant(HasScore, std::nullopt);
  builder.ExpectVariant(HasCodecs, std::nullopt);
  builder.ExpectVariant(
      HasResolution, types::DecimalResolution{.width = 1920, .height = 1080});
  builder.ExpectVariant(HasFrameRate, std::nullopt);
  builder.ExpectOk();

  // Check the value of the 'FRAME-RATE' attribute
  builder.AppendLine(R"(#EXT-X-STREAM-INF:BANDWIDTH=110,FRAME-RATE=59.94)");
  builder.AppendLine("playlist9.m3u8");
  builder.ExpectAdditionalVariant();
  builder.ExpectVariant(HasPrimaryRenditionUri,
                        GURL("http://localhost/playlist9.m3u8"));
  builder.ExpectVariant(HasBandwidth, 110u);
  builder.ExpectVariant(HasAverageBandwidth, std::nullopt);
  builder.ExpectVariant(HasScore, std::nullopt);
  builder.ExpectVariant(HasCodecs, std::nullopt);
  builder.ExpectVariant(HasResolution, std::nullopt);
  builder.ExpectVariant(HasFrameRate, 59.94);
  builder.ExpectOk();
}

TEST(HlsMultivariantPlaylistTest, XMediaTag) {
  MultivariantPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");

  // Invalid EXT-X-MEDIA tags should cause the playlist to be rejected
  auto fork = builder;
  fork.AppendLine("#EXT-X-MEDIA");
  fork.ExpectError(ParseStatusCode::kMalformedTag);
  fork = builder;
  fork.AppendLine("#EXT-X-MEDIA:TYPE=AUDIO");
  fork.ExpectError(ParseStatusCode::kMalformedTag);

  // Rendition group ids that are referenced by EXT-X-STREAM-INF tags but not by
  // EXT-X-MEDIA tags should result in an error.
  fork = builder;
  fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=100,AUDIO=\"foo\"");
  fork.AppendLine("playlist.m3u8");
  fork.ExpectError(ParseStatusCode::kRenditionGroupDoesNotExist);

  // ... But not the other way around
  fork = builder;
  fork.AppendLine(
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"foo\",NAME=\"English\",URI=\"english."
      "m3u8\"");
  fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=100");
  fork.AppendLine("playlist.m3u8");
  fork.ExpectAdditionalVariant();
  fork.ExpectVariant(HasAudioRenditionGroup, std::nullopt);
  fork.ExpectOk();

  // Rendition groups may be referenced by EXT-X-STREAM-INF and EXT-X-MEDIA tags
  // in any order.
  fork = builder;
  fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=100,AUDIO=\"foo\"");
  fork.AppendLine("playlist.m3u8");
  fork.ExpectAdditionalVariant();
  fork.ExpectVariant(HasAudioRenditionGroup, "foo");
  fork.AppendLine(
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"foo\",NAME=\"English\",URI=\"english."
      "m3u8\"");
  fork.ExpectAudioRendition("foo", "English", RenditionHasUri,
                            GURL("http://localhost/english.m3u8"));
  fork.ExpectAudioRendition("foo", "English", HasLanguage, std::nullopt);
  fork.ExpectAudioRendition("foo", "English", HasAssociatedLanguage,
                            std::nullopt);
  fork.ExpectAudioRendition("foo", "English", MayAutoSelect, false);
  fork.ExpectAudioRendition("foo", "English", HasStableRenditionId,
                            std::nullopt);
  fork.ExpectAudioRenditionGroup("foo", HasDefaultRendition, std::nullopt);
  fork.ExpectOk();

  fork = builder;
  fork.AppendLine(
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"foo\",NAME=\"English\",URI=\"english."
      "m3u8\"");
  fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=100,AUDIO=\"foo\"");
  fork.AppendLine("playlist.m3u8");
  fork.ExpectAdditionalVariant();
  fork.ExpectVariant(HasAudioRenditionGroup, "foo");
  fork.ExpectAudioRendition("foo", "English", RenditionHasUri,
                            GURL("http://localhost/english.m3u8"));
  fork.ExpectAudioRendition("foo", "English", HasLanguage, std::nullopt);
  fork.ExpectAudioRendition("foo", "English", HasAssociatedLanguage,
                            std::nullopt);
  fork.ExpectAudioRendition("foo", "English", MayAutoSelect, false);
  fork.ExpectAudioRendition("foo", "English", HasStableRenditionId,
                            std::nullopt);
  fork.ExpectAudioRenditionGroup("foo", HasDefaultRendition, std::nullopt);
  fork.ExpectOk();

  // Two EXT-X-MEDIA tags in the same group may not have the same name
  fork = builder;
  fork.AppendLine(
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"foo\",NAME=\"English\",URI=\"english."
      "m3u8\"");
  fork.AppendLine(
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"foo\",NAME=\"English\",URI="
      "\"english2.m3u8\"");
  fork.ExpectError(ParseStatusCode::kRenditionGroupHasDuplicateRenditionNames);

  // .. Unless they are in different groups
  fork = builder;
  fork.AppendLine(
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"foo\",NAME=\"English\",URI=\"english."
      "m3u8\"");
  fork.AppendLine(
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"bar\",NAME=\"English\",URI="
      "\"english2.m3u8\"");
  fork.ExpectOk();

  // Despite what the spec says, multiple renditions in the same group
  // frequently have DEFAULT=YES, so we allow that. Only the first to appear is
  // considered the default.
  fork = builder;
  fork.AppendLine(
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"foo\",NAME=\"English\",URI=\"english."
      "m3u8\",DEFAULT=YES");
  fork.AppendLine(
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"foo\",NAME=\"Spanish\",URI=\"spanish."
      "m3u8\",DEFAULT=YES");
  fork.ExpectAudioRenditionGroup("foo", HasDefaultRendition, "English");
  fork.ExpectAudioRendition("foo", "English", RenditionHasUri,
                            GURL("http://localhost/english.m3u8"));
  fork.ExpectAudioRendition("foo", "English", MayAutoSelect, true);
  fork.ExpectAudioRendition("foo", "Spanish", RenditionHasUri,
                            GURL("http://localhost/spanish.m3u8"));
  fork.ExpectAudioRendition("foo", "Spanish", MayAutoSelect, true);
  fork.AppendLine("#EXT-X-STREAM-INF:BANDWIDTH=100,AUDIO=\"foo\"");
  fork.AppendLine("stream1.m3u8");
  fork.ExpectAdditionalVariant();
  fork.ExpectVariant(HasAudioRenditionGroup, "foo");
  fork.ExpectOk();
}

}  // namespace media::hls
