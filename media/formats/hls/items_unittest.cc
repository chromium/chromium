// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/items.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace media {
namespace hls {

namespace {

using LineResult = GetNextLineItemResult;

// Calls `GetNextLineItem` for each expectation, and verifies that the result
// matches.
template <typename T>
void RunTest(base::StringPiece manifest, const T& expectations) {
  size_t line_number = 1;

  for (auto expectation : expectations) {
    auto result = GetNextLineItem(&manifest, &line_number);

    if (expectation.has_value()) {
      auto expected_value = std::move(expectation).value();

      EXPECT_TRUE(result.has_value());
      auto value = std::move(result).value();

      // Ensure that resulting variants are the same
      static_assert(absl::variant_size<LineResult>::value == 2, "");
      if (auto* expected_tag = absl::get_if<TagItem>(&expected_value)) {
        auto tag = absl::get<TagItem>(std::move(value));
        EXPECT_EQ(expected_tag->kind, tag.kind);
        EXPECT_EQ(expected_tag->line_number, tag.line_number);
        EXPECT_EQ(expected_tag->content, tag.content);
      } else {
        auto expected_uri = absl::get<UriItem>(std::move(expected_value));
        auto uri = absl::get<UriItem>(std::move(value));
        EXPECT_EQ(expected_uri.line_number, uri.line_number);
        EXPECT_EQ(expected_uri.text, uri.text);
      }
    } else {
      EXPECT_TRUE(result.has_error());
      auto error = std::move(result).error();
      auto expected_error = std::move(expectation).error();
      EXPECT_EQ(error.code(), expected_error.code());
    }
  }
}

}  // namespace

TEST(HlsFormatParserTest, GetNextLineItemTest1) {
  constexpr base::StringPiece kManifest =
      "#EXTM3U\n"
      "\n"
      "#ExTm3U\n"
      "# EXTM3U\n"
      "#EXTasdf\n"
      "##Comment\n"
      "EXTM3U\n"
      "\r\n"
      "# Comment\n"
      "http://www.example.com\n"
      "../media.m3u8\n"
      "foobar.jpg\n"
      " video about food.mov\r\n"
      "uri_with_{$variable}.mov\r\n"
      "#EXT-X-VERSION:7\n"
      "#EXT-X-VERSION:\n"
      "#EXTINF:1234,\t\n";

  const ParseStatus::Or<LineResult> kExpectations[] = {
      LineResult(TagItem(TagKind::kM3u, 1, "")),
      // Unknown tag content should be entire line following "#EXT"
      LineResult(TagItem(TagKind::kUnknown, 5, "asdf")),
      // Lines without leading # should be considered URIs
      LineResult(UriItem(7, "EXTM3U")),
      LineResult(UriItem(10, "http://www.example.com")),
      LineResult(UriItem(11, "../media.m3u8")),
      LineResult(UriItem(12, "foobar.jpg")),
      // Whitespace is not allowed here, but that's not this function's
      // responsibility.
      LineResult(UriItem(13, " video about food.mov")),
      // Variable substitution is not this function's responsibility.
      LineResult(UriItem(14, "uri_with_{$variable}.mov")),
      LineResult(TagItem(TagKind::kXVersion, 15, "7")),
      LineResult(TagItem(TagKind::kXVersion, 16, "")),
      LineResult(TagItem(TagKind::kInf, 17, "1234,\t")),
      ParseStatusCode::kReachedEOF, ParseStatusCode::kReachedEOF};

  RunTest(kManifest, kExpectations);
}

TEST(HlsFormatParserTest, GetNextLineItemTest2) {
  constexpr base::StringPiece kManifest =
      "#EXTM3U\n"
      "https://ww\rw.example.com\n"
      "#EXT-X-VERSION:3\n";

  const ParseStatus::Or<LineResult> kExpectations[] = {
      LineResult(TagItem(TagKind::kM3u, 1, "")), ParseStatusCode::kInvalidEOL,
      ParseStatusCode::kInvalidEOL};

  RunTest(kManifest, kExpectations);
}

TEST(HlsFormatParserTest, GetNextLineItemTest3) {
  constexpr base::StringPiece kManifest = "#EXTM3U";

  const ParseStatus::Or<LineResult> kExpectations[] = {
      ParseStatusCode::kInvalidEOL, ParseStatusCode::kInvalidEOL};

  RunTest(kManifest, kExpectations);
}

TEST(HlsFormatParserTest, GetNextLineItemTest4) {
  constexpr base::StringPiece kManifest = "#EXTM3U\r";

  const ParseStatus::Or<LineResult> kExpectations[] = {
      ParseStatusCode::kInvalidEOL, ParseStatusCode::kInvalidEOL};

  RunTest(kManifest, kExpectations);
}

TEST(HlsFormatParserTest, GetNextLineItemTest5) {
  constexpr base::StringPiece kManifest = "\n";

  const ParseStatus::Or<LineResult> kExpectations[] = {
      ParseStatusCode::kReachedEOF, ParseStatusCode::kReachedEOF};

  RunTest(kManifest, kExpectations);
}

}  // namespace hls
}  // namespace media
