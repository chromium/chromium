// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/items.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace media::hls {

namespace {

using LineResult = GetNextLineItemResult;

void CheckSourceString(SourceString expected, SourceString actual) {
  EXPECT_EQ(expected.Line(), actual.Line());
  EXPECT_EQ(expected.Column(), actual.Column());
  EXPECT_EQ(expected.Str(), actual.Str());
}

// Calls `GetNextLineItem` for each expectation, and verifies that the result
// matches.
template <typename T>
void RunTest(base::StringPiece source, const T& expectations) {
  auto line_iter = SourceLineIterator(source);

  for (auto expectation : expectations) {
    auto result = GetNextLineItem(&line_iter);

    if (expectation.has_value()) {
      auto expected_value = std::move(expectation).value();

      EXPECT_TRUE(result.has_value());
      auto value = std::move(result).value();

      // Ensure that resulting variants are the same
      static_assert(absl::variant_size<LineResult>::value == 2, "");
      if (auto* expected_tag = absl::get_if<TagItem>(&expected_value)) {
        auto tag = absl::get<TagItem>(std::move(value));
        EXPECT_EQ(expected_tag->name, tag.name);
        CheckSourceString(expected_tag->content, tag.content);
      } else {
        auto expected_uri = absl::get<UriItem>(std::move(expected_value));
        auto uri = absl::get<UriItem>(std::move(value));
        CheckSourceString(expected_uri.content, uri.content);
      }
    } else {
      EXPECT_TRUE(result.has_error());
      auto error = std::move(result).error();
      auto expected_error = std::move(expectation).error();
      EXPECT_EQ(error.code(), expected_error.code());
    }
  }
}

template <typename T>
ParseStatus::Or<LineResult> ExpectTag(T name,
                                      size_t line,
                                      size_t col,
                                      base::StringPiece content) {
  return LineResult(
      TagItem{.name = ToTagName(name),
              .content = SourceString::CreateForTesting(line, col, content)});
}

ParseStatus::Or<LineResult> ExpectUri(size_t line,
                                      size_t col,
                                      base::StringPiece content) {
  return LineResult(
      UriItem{.content = SourceString::CreateForTesting(line, col, content)});
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
      ExpectTag(CommonTagName::kM3u, 1, 8, ""),
      // Unknown tag content should be entire line following "#EXT"
      ExpectTag(kUnknownTagName, 5, 5, "asdf"),

      // Lines without leading # should be considered URIs
      ExpectUri(7, 1, "EXTM3U"), ExpectUri(10, 1, "http://www.example.com"),
      ExpectUri(11, 1, "../media.m3u8"), ExpectUri(12, 1, "foobar.jpg"),

      // Whitespace is not allowed here, but that's not this function's
      // responsibility.
      ExpectUri(13, 1, " video about food.mov"),

      // Variable substitution is not this function's responsibility.
      ExpectUri(14, 1, "uri_with_{$variable}.mov"),

      ExpectTag(CommonTagName::kXVersion, 15, 16, "7"),
      ExpectTag(CommonTagName::kXVersion, 16, 16, ""),
      ExpectTag(MediaPlaylistTagName::kInf, 17, 9, "1234,\t"),
      ParseStatusCode::kReachedEOF, ParseStatusCode::kReachedEOF};

  RunTest(kManifest, kExpectations);
}

TEST(HlsFormatParserTest, GetNextLineItemTest2) {
  constexpr base::StringPiece kManifest =
      "#EXTM3U\n"
      "https://ww\rw.example.com\n"
      "#EXT-X-VERSION:3\n";

  const ParseStatus::Or<LineResult> kExpectations[] = {
      ExpectTag(CommonTagName::kM3u, 1, 8, ""), ParseStatusCode::kInvalidEOL,
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

}  // namespace media::hls
