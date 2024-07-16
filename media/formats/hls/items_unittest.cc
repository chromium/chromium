// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/items.h"

#include <string_view>

#include "base/location.h"
#include "media/formats/hls/source_string.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace media::hls {

namespace {

using LineResult = GetNextLineItemResult;

void CheckSourceString(SourceString expected,
                       SourceString actual,
                       const base::Location& from) {
  EXPECT_EQ(expected.Line(), actual.Line()) << from.ToString();
  EXPECT_EQ(expected.Column(), actual.Column()) << from.ToString();
  EXPECT_EQ(expected.Str(), actual.Str()) << from.ToString();
}

// Calls `GetNextLineItem` for each expectation, and verifies that the result
// matches.
template <typename T>
void RunTest(std::string_view source,
             const T& expectations,
             const base::Location& from = base::Location::Current()) {
  auto line_iter = SourceLineIterator(source);

  for (auto expectation : expectations) {
    auto result = GetNextLineItem(&line_iter);

    if (expectation.has_value()) {
      auto expected_value = std::move(expectation).value();

      EXPECT_TRUE(result.has_value()) << from.ToString();
      auto value = std::move(result).value();

      // Ensure that resulting variants are the same
      static_assert(absl::variant_size<LineResult>::value == 2, "");
      if (auto* expected_tag = absl::get_if<TagItem>(&expected_value)) {
        auto tag = absl::get<TagItem>(std::move(value));
        EXPECT_EQ(expected_tag->GetName(), tag.GetName()) << from.ToString();
        EXPECT_EQ(expected_tag->GetLineNumber(), tag.GetLineNumber())
            << from.ToString();
        EXPECT_EQ(expected_tag->GetContent().has_value(),
                  tag.GetContent().has_value())
            << from.ToString();
        if (expected_tag->GetContent().has_value() &&
            tag.GetContent().has_value()) {
          CheckSourceString(*expected_tag->GetContent(), *tag.GetContent(),
                            from);
        }
      } else {
        auto expected_uri = absl::get<UriItem>(std::move(expected_value));
        auto uri = absl::get<UriItem>(std::move(value));
        CheckSourceString(expected_uri.content, uri.content, from);
      }
    } else {
      EXPECT_FALSE(result.has_value()) << from.ToString();
      auto error = std::move(result).error();
      auto expected_error = std::move(expectation).error();
      EXPECT_EQ(error.code(), expected_error.code()) << from.ToString();
    }
  }
}

template <typename T>
ParseStatus::Or<LineResult> ExpectTag(T name,
                                      size_t line,
                                      size_t col,
                                      std::string_view content) {
  return LineResult(TagItem::Create(
      ToTagName(name), SourceString::CreateForTesting(line, col, content)));
}

template <typename T>
ParseStatus::Or<LineResult> ExpectEmptyTag(T name, size_t line) {
  return LineResult(TagItem::CreateEmpty(ToTagName(name), line));
}

ParseStatus::Or<LineResult> ExpectUnknownTag(std::string_view name,
                                             size_t line) {
  return LineResult(
      TagItem::CreateUnknown(SourceString::CreateForTesting(line, 2, name)));
}

ParseStatus::Or<LineResult> ExpectUri(size_t line,
                                      size_t col,
                                      std::string_view content) {
  return LineResult(
      UriItem{.content = SourceString::CreateForTesting(line, col, content)});
}

}  // namespace

TEST(HlsItemsTest, GetNextLineItem1) {
  constexpr std::string_view kManifest =
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
      "#EXT-X-VERSION\n"
      "#EXT-X-VERSION-FOO\n"
      "#EXTINF:1234,\t\n";

  const ParseStatus::Or<LineResult> kExpectations[] = {
      ExpectEmptyTag(CommonTagName::kM3u, 1), ExpectUnknownTag("EXTasdf", 5),

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
      ExpectEmptyTag(CommonTagName::kXVersion, 17),
      ExpectUnknownTag("EXT-X-VERSION-FOO", 18),
      ExpectTag(MediaPlaylistTagName::kInf, 19, 9, "1234,\t"),
      ParseStatusCode::kReachedEOF, ParseStatusCode::kReachedEOF};

  RunTest(kManifest, kExpectations);
}

TEST(HlsItemsTest, GetNextLineItemAcceptMissingEOL) {
  constexpr std::string_view kManifest = "#EXTM3U";

  const ParseStatus::Or<LineResult> kExpectations[] = {
      ExpectEmptyTag(CommonTagName::kM3u, 1), ParseStatusCode::kReachedEOF};

  RunTest(kManifest, kExpectations);
}

TEST(HlsItemsTest, GetNextLineItem2) {
  constexpr std::string_view kManifest =
      "#EXTM3U\n"
      "https://ww\rw.example.com\n"
      "#EXT-X-VERSION:3\n";

  const ParseStatus::Or<LineResult> kExpectations[] = {
      ExpectEmptyTag(CommonTagName::kM3u, 1), ParseStatusCode::kInvalidEOL,
      ParseStatusCode::kInvalidEOL};

  RunTest(kManifest, kExpectations);
}

TEST(HlsItemsTest, GetNextLineItem4) {
  constexpr std::string_view kManifest = "#EXTM3U\r";

  const ParseStatus::Or<LineResult> kExpectations[] = {
      ParseStatusCode::kInvalidEOL, ParseStatusCode::kInvalidEOL};

  RunTest(kManifest, kExpectations);
}

TEST(HlsItemsTest, GetNextLineItem5) {
  constexpr std::string_view kManifest = "\n";

  const ParseStatus::Or<LineResult> kExpectations[] = {
      ParseStatusCode::kReachedEOF, ParseStatusCode::kReachedEOF};

  RunTest(kManifest, kExpectations);
}

}  // namespace media::hls
