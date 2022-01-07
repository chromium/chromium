// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/tags.h"
#include "media/formats/hls/parse_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace hls {

template <typename T>
void ErrorTest(SourceString content, ParseStatusCode expected_status) {
  auto tag = TagItem{.kind = T::kKind, .content = content};
  auto result = T::Parse(tag);
  EXPECT_TRUE(result.has_error());
  auto error = std::move(result).error();
  EXPECT_EQ(error.code(), expected_status);
}

template <typename T>
T OkTest(SourceString content) {
  auto tag = TagItem{.kind = T::kKind, .content = content};
  auto result = T::Parse(tag);
  EXPECT_TRUE(result.has_value());
  return std::move(result).value();
}

TEST(HlsFormatParserTest, ParseM3uTagTest) {
  // Empty content is the only allowed content
  OkTest<M3uTag>(SourceString::CreateForTesting(""));

  // Test with non-empty content
  ErrorTest<M3uTag>(SourceString::CreateForTesting(" "),
                    ParseStatusCode::kMalformedTag);
  ErrorTest<M3uTag>(SourceString::CreateForTesting("a"),
                    ParseStatusCode::kMalformedTag);
  ErrorTest<M3uTag>(SourceString::CreateForTesting("1234"),
                    ParseStatusCode::kMalformedTag);
  ErrorTest<M3uTag>(SourceString::CreateForTesting("\t"),
                    ParseStatusCode::kMalformedTag);
}

TEST(HlsFormatParserTest, ParseXVersionTagTest) {
  // Test valid versions
  XVersionTag tag = OkTest<XVersionTag>(SourceString::CreateForTesting("1"));
  EXPECT_EQ(tag.version, 1u);
  tag = OkTest<XVersionTag>(SourceString::CreateForTesting("2"));
  EXPECT_EQ(tag.version, 2u);
  tag = OkTest<XVersionTag>(SourceString::CreateForTesting("3"));
  EXPECT_EQ(tag.version, 3u);
  tag = OkTest<XVersionTag>(SourceString::CreateForTesting("4"));
  EXPECT_EQ(tag.version, 4u);
  tag = OkTest<XVersionTag>(SourceString::CreateForTesting("5"));
  EXPECT_EQ(tag.version, 5u);
  tag = OkTest<XVersionTag>(SourceString::CreateForTesting("6"));
  EXPECT_EQ(tag.version, 6u);
  tag = OkTest<XVersionTag>(SourceString::CreateForTesting("7"));
  EXPECT_EQ(tag.version, 7u);
  tag = OkTest<XVersionTag>(SourceString::CreateForTesting("8"));
  EXPECT_EQ(tag.version, 8u);
  tag = OkTest<XVersionTag>(SourceString::CreateForTesting("9"));
  EXPECT_EQ(tag.version, 9u);
  tag = OkTest<XVersionTag>(SourceString::CreateForTesting("10"));
  EXPECT_EQ(tag.version, 10u);

  // While unsupported playlist versions are rejected, that's NOT the
  // responsibility of this tag parsing function. The playlist should be
  // rejected at a higher level.
  tag = OkTest<XVersionTag>(SourceString::CreateForTesting("99999"));
  EXPECT_EQ(tag.version, 99999u);

  // Test invalid versions
  ErrorTest<XVersionTag>(SourceString::CreateForTesting(""),
                         ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>(SourceString::CreateForTesting("0"),
                         ParseStatusCode::kInvalidPlaylistVersion);
  ErrorTest<XVersionTag>(SourceString::CreateForTesting("-1"),
                         ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>(SourceString::CreateForTesting("1.0"),
                         ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>(SourceString::CreateForTesting("asdf"),
                         ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>(SourceString::CreateForTesting("  1 "),
                         ParseStatusCode::kMalformedTag);
}

TEST(HlsFormatParserTest, ParseInfTagTest) {
  // Test some valid tags
  InfTag tag = OkTest<InfTag>(SourceString::CreateForTesting("1234,"));
  EXPECT_EQ(tag.duration, 1234.0);
  EXPECT_EQ(tag.title.Str(), "");

  tag = OkTest<InfTag>(SourceString::CreateForTesting("1.234,"));
  EXPECT_EQ(tag.duration, 1.234);
  EXPECT_EQ(tag.title.Str(), "");

  // The spec implies that whitespace characters like this usually aren't
  // permitted, but "\t" is a common occurrence for the title value.
  tag = OkTest<InfTag>(SourceString::CreateForTesting("99.5,\t"));
  EXPECT_EQ(tag.duration, 99.5);
  EXPECT_EQ(tag.title.Str(), "\t");

  tag = OkTest<InfTag>(SourceString::CreateForTesting("9.5,,,,"));
  EXPECT_EQ(tag.duration, 9.5);
  EXPECT_EQ(tag.title.Str(), ",,,");

  tag = OkTest<InfTag>(SourceString::CreateForTesting("12,asdfsdf   "));
  EXPECT_EQ(tag.duration, 12.0);
  EXPECT_EQ(tag.title.Str(), "asdfsdf   ");

  // Test some invalid tags
  ErrorTest<InfTag>(SourceString::CreateForTesting(""),
                    ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>(SourceString::CreateForTesting(","),
                    ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>(SourceString::CreateForTesting("-123,"),
                    ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>(SourceString::CreateForTesting("123"),
                    ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>(SourceString::CreateForTesting("asdf,"),
                    ParseStatusCode::kMalformedTag);
}

}  // namespace hls
}  // namespace media
