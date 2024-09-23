// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_playlist_test_builder.h"
#include "media/formats/hls/multivariant_playlist_test_builder.h"
#include "media/formats/hls/parse_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

namespace {

template <typename BuilderT>
class HlsCommonPlaylistTest : public testing::Test {
 public:
  using Builder = BuilderT;
};

using Implementations =
    testing::Types<MultivariantPlaylistTestBuilder, MediaPlaylistTestBuilder>;
TYPED_TEST_SUITE(HlsCommonPlaylistTest, Implementations);

// Adds any additional tags not relevant to these tests, but that are required
// for this playlist type to be successfully parsed.
void AddRequiredTags(MultivariantPlaylistTestBuilder&) {
  // None required
}
void AddRequiredTags(MediaPlaylistTestBuilder& builder) {
  builder.AppendLine("#EXT-X-TARGETDURATION:10");
}

}  // namespace

TYPED_TEST(HlsCommonPlaylistTest, BadLineEndings) {
  TypeParam builder;
  builder.AppendLine("#EXTM3U");
  AddRequiredTags(builder);

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

TYPED_TEST(HlsCommonPlaylistTest, MissingM3u) {
  // #EXTM3U must be the very first line
  TypeParam builder;
  builder.AppendLine("");
  builder.AppendLine("#EXTM3U");
  builder.ExpectError(ParseStatusCode::kPlaylistMissingM3uTag);

  builder = TypeParam();
  builder.AppendLine("#EXT-X-VERSION:5");
  builder.AppendLine("#EXTM3U");
  builder.ExpectError(ParseStatusCode::kPlaylistMissingM3uTag);

  // Test with invalid format
  builder = TypeParam();
  builder.AppendLine("#EXTM3U:");
  builder.ExpectError(ParseStatusCode::kPlaylistMissingM3uTag);
  builder = TypeParam();
  builder.AppendLine("#EXTM3U:1");
  builder.ExpectError(ParseStatusCode::kPlaylistMissingM3uTag);

  // Extra M3U tag is OK
  builder = TypeParam();
  builder.AppendLine("#EXTM3U");
  builder.AppendLine("#EXTM3U");
  AddRequiredTags(builder);
  builder.ExpectOk();
}

TYPED_TEST(HlsCommonPlaylistTest, UnknownTag) {
  TypeParam builder;
  builder.AppendLine("#EXTM3U");
  AddRequiredTags(builder);

  // Unrecognized tags should not result in an error
  builder.AppendLine("#EXT-UNKNOWN-TAG");
  builder.ExpectOk();
}

TYPED_TEST(HlsCommonPlaylistTest, VersionChecks) {
  TypeParam builder;
  builder.AppendLine("#EXTM3U");
  AddRequiredTags(builder);

  {
    // Default version is 1
    auto fork = builder;
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
    fork.SetVersion(i);
    fork.ExpectOk();
  }

  for (int i : {11, 12, 100, 999}) {
    // Versions 11+ are not supported by this implementation
    auto fork = builder;
    fork.AppendLine("#EXT-X-VERSION:" + base::NumberToString(i));
    fork.SetVersion(i);
    fork.ExpectError(ParseStatusCode::kPlaylistHasUnsupportedVersion);
  }

  // Version must match what's expected
  builder.SetVersion(5);
  builder.ExpectError(ParseStatusCode::kPlaylistHasVersionMismatch);

  builder.AppendLine("#EXT-X-VESION:3");
  builder.ExpectError(ParseStatusCode::kPlaylistHasVersionMismatch);
}

TYPED_TEST(HlsCommonPlaylistTest, XIndependentSegmentsTag) {
  TypeParam builder;
  builder.AppendLine("#EXTM3U");
  AddRequiredTags(builder);

  // Without the 'EXT-X-INDEPENDENT-SEGMENTS' tag, the default is 'false'.
  {
    auto fork = builder;
    fork.ExpectPlaylist(HasIndependentSegments, false);
    fork.ExpectOk();
  }

  builder.AppendLine("#EXT-X-INDEPENDENT-SEGMENTS");
  builder.ExpectPlaylist(HasIndependentSegments, true);
  builder.ExpectOk();

  // This tag should not appear twice
  builder.AppendLine("#EXT-X-INDEPENDENT-SEGMENTS");
  builder.ExpectError(ParseStatusCode::kPlaylistHasDuplicateTags);
}

}  // namespace media::hls
