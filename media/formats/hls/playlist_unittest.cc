// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/playlist.h"

#include <string_view>

#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/tag_name.h"
#include "media/formats/hls/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

TEST(HlsPlaylistTest, IdentifyPlaylist) {
  constexpr auto ok_test = [](types::DecimalInteger version,
                              Playlist::Kind kind, std::string_view src,
                              const base::Location& from =
                                  base::Location::Current()) {
    auto result = Playlist::IdentifyPlaylist(src);
    ASSERT_TRUE(result.has_value()) << from.ToString();

    auto ident = std::move(result).value();
    EXPECT_EQ(ident.version, version) << from.ToString();
    EXPECT_EQ(ident.kind, kind) << from.ToString();
  };

  constexpr auto error_test =
      [](ParseStatusCode expected_error, std::string_view src,
         const base::Location& from = base::Location::Current()) {
        auto result = Playlist::IdentifyPlaylist(src);
        ASSERT_FALSE(result.has_value()) << from.ToString();

        auto error = std::move(result).error();
        EXPECT_EQ(error.code(), expected_error) << from.ToString();
      };

  // A completely empty playlist should be identified as a multivariant playlist
  // with the default version. This will obviously fail to parse as a
  // multivariant playlist, but the goal of this function is to do the minimum
  // work necessary to determine the version and disambiguate a media playlist
  // from a multivariant playlist, not necessarily a valid playlist from an
  // invalid playlist. There are many reasons a playlist might be invalid, so
  // that's best left to the actual parsing function.
  ok_test(Playlist::kDefaultVersion, Playlist::Kind::kMultivariantPlaylist, "");
  ok_test(5, Playlist::Kind::kMultivariantPlaylist, "#EXT-X-VERSION:5\n");

  // Playlists with invalid line endings should normally be rejected, however
  // other implementations in certain browsers do accept manifests which are
  // missing a trailing newline.
  ok_test(Playlist::kDefaultVersion, Playlist::Kind::kMediaPlaylist, "#EXTINF");

  // Playlists with kind-specific tags should deduce to that kind of playlist.
  // These tags do not need to be valid.
  for (TagName tag = kMinTagName; tag <= kMaxTagName; ++tag) {
    if (tag == ToTagName(CommonTagName::kXVersion)) {
      continue;
    }

    // Test with a couple different version numbers
    for (types::DecimalInteger version : {0, 1, 5, 10}) {
      // Test with and without a common tag
      for (bool common_tag : {true, false}) {
        std::string src;

        if (common_tag) {
          src += "#EXTM3U\n";
        }

        if (version) {
          src += "#EXT-X-VERSION:" + base::NumberToString(version) + "\n";
        }

        src += "#" + std::string(TagNameToString(tag)) + "\n";

        switch (GetTagKind(tag)) {
          case TagKind::kCommonTag:
            ok_test(version ? version : Playlist::kDefaultVersion,
                    Playlist::Kind::kMultivariantPlaylist, src);
            break;
          case TagKind::kMultivariantPlaylistTag:
            ok_test(version ? version : Playlist::kDefaultVersion,
                    Playlist::Kind::kMultivariantPlaylist, src);
            break;
          case TagKind::kMediaPlaylistTag:
            ok_test(version ? version : Playlist::kDefaultVersion,
                    Playlist::Kind::kMediaPlaylist, src);
            break;
        }
      }
    }
  }

  // Invalid or unsupported versions should result in an error
  error_test(ParseStatusCode::kMalformedTag, "#EXT-X-VERSION:-1\n");
  error_test(ParseStatusCode::kInvalidPlaylistVersion, "#EXT-X-VERSION:0\n");
  error_test(ParseStatusCode::kPlaylistHasUnsupportedVersion,
             "#EXT-X-VERSION:11\n");

  // Conflicting tag kinds should result in an error
  error_test(ParseStatusCode::kMultivariantPlaylistHasMediaPlaylistTag,
             "#EXT-X-STREAM-INF\n#EXTINF\n");
  error_test(ParseStatusCode::kMediaPlaylistHasMultivariantPlaylistTag,
             "#EXTINF\n#EXT-X-STREAM-INF\n");

  // ...unless the error occurs after this function has determined version and
  // playlist kind
  ok_test(5, Playlist::Kind::kMediaPlaylist,
          "#EXT-X-VERSION:5\n#EXTINF\n#EXT-X-STREAM-INF\n");

  // Duplicate or conflicting version tags should result in an error
  error_test(ParseStatusCode::kPlaylistHasDuplicateTags,
             "#EXT-X-VERSION:3\n#EXT-X-VERSION:3\n");
  error_test(ParseStatusCode::kPlaylistHasDuplicateTags,
             "#EXT-X-VERSION:3\n#EXT-X-VERSION:4\n");

  // ...unless the error occurs after this function has determined version and
  // playlist kind
  ok_test(5, Playlist::Kind::kMediaPlaylist,
          "#EXT-X-VERSION:5\n#EXTINF\n#EXT-X-VERSION:6\n");

  // Unknown tags should not affect this function
  ok_test(5, Playlist::Kind::kMediaPlaylist,
          "#EXT-X-VERSION:5\n#EXT-X-FAKE-TAG\n#EXTINF\n");
}

}  // namespace media::hls
