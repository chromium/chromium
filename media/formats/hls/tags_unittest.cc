// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/tags.h"

#include <functional>
#include <utility>

#include "base/location.h"
#include "base/strings/string_piece.h"
#include "media/formats/hls/items.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/test_util.h"
#include "media/formats/hls/variable_dictionary.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media::hls {

namespace {

template <typename T>
void ErrorTest(absl::optional<base::StringPiece> content,
               ParseStatusCode expected_status,
               const base::Location& from = base::Location::Current()) {
  auto tag = content ? TagItem::Create(ToTagName(T::kName),
                                       SourceString::CreateForTesting(*content))
                     : TagItem::CreateEmpty(ToTagName(T::kName), 1);
  auto result = T::Parse(tag);
  ASSERT_TRUE(result.has_error()) << from.ToString();
  auto error = std::move(result).error();
  EXPECT_EQ(error.code(), expected_status) << from.ToString();
}

template <typename T>
void ErrorTest(absl::optional<base::StringPiece> content,
               const VariableDictionary& variable_dict,
               VariableDictionary::SubstitutionBuffer& sub_buffer,
               ParseStatusCode expected_status,
               const base::Location& from = base::Location::Current()) {
  auto tag = content ? TagItem::Create(ToTagName(T::kName),
                                       SourceString::CreateForTesting(*content))
                     : TagItem::CreateEmpty(ToTagName(T::kName), 1);
  auto result = T::Parse(tag, variable_dict, sub_buffer);
  ASSERT_TRUE(result.has_error()) << from.ToString();
  auto error = std::move(result).error();
  EXPECT_EQ(error.code(), expected_status) << from.ToString();
}

template <typename T>
T OkTest(absl::optional<base::StringPiece> content,
         const base::Location& from = base::Location::Current()) {
  auto tag = content ? TagItem::Create(ToTagName(T::kName),
                                       SourceString::CreateForTesting(*content))
                     : TagItem::CreateEmpty(ToTagName(T::kName), 1);
  auto result = T::Parse(tag);
  EXPECT_TRUE(result.has_value()) << from.ToString();
  return std::move(result).value();
}

template <typename T>
T OkTest(absl::optional<base::StringPiece> content,
         const VariableDictionary& variable_dict,
         VariableDictionary::SubstitutionBuffer& sub_buffer,
         const base::Location& from = base::Location::Current()) {
  auto tag = content ? TagItem::Create(ToTagName(T::kName),
                                       SourceString::CreateForTesting(*content))
                     : TagItem::CreateEmpty(ToTagName(T::kName), 1);
  auto result = T::Parse(tag, variable_dict, sub_buffer);
  CHECK(result.has_value()) << from.ToString();
  return std::move(result).value();
}

// Helper to test identification of this tag in a manifest.
// `line` must be a sample line containing this tag, and must end with a
// newline. This DOES NOT parse the item content (only that the item content
// matches what was expected), use `OkTest` and `ErrorTest` for that.
template <typename T>
void RunTagIdenficationTest(
    base::StringPiece line,
    absl::optional<base::StringPiece> expected_content,
    const base::Location& from = base::Location::Current()) {
  auto iter = SourceLineIterator(line);
  auto item_result = GetNextLineItem(&iter);
  ASSERT_TRUE(item_result.has_value()) << from.ToString();

  auto item = std::move(item_result).value();
  auto* tag = absl::get_if<TagItem>(&item);
  ASSERT_NE(tag, nullptr) << from.ToString();
  EXPECT_EQ(tag->GetName(), ToTagName(T::kName)) << from.ToString();
  EXPECT_EQ(tag->GetContent().has_value(), expected_content.has_value())
      << from.ToString();
  if (tag->GetContent().has_value() && expected_content.has_value()) {
    EXPECT_EQ(tag->GetContent()->Str(), *expected_content) << from.ToString();
  }
}

// Test helper for tags which are expected to have no content
template <typename T>
void RunEmptyTagTest() {
  // Empty content is the only allowed content
  OkTest<T>(absl::nullopt);

  // Test with non-empty content
  ErrorTest<T>("", ParseStatusCode::kMalformedTag);
  ErrorTest<T>(" ", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("a", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("1234", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("\t", ParseStatusCode::kMalformedTag);
}

// There are a couple of tags that are defined simply as `#EXT-X-TAG:n` where
// `n` must be a valid DecimalInteger. This helper provides coverage for those
// tags.
template <typename T, typename Fn>
void RunDecimalIntegerTagTest(Fn getter_fn) {
  // Content is required
  ErrorTest<T>(absl::nullopt, ParseStatusCode::kMalformedTag);
  ErrorTest<T>("", ParseStatusCode::kMalformedTag);

  // Content must be a valid decimal-integer
  ErrorTest<T>("-1", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("-1.5", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("-.5", ParseStatusCode::kMalformedTag);
  ErrorTest<T>(".5", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("0.5", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("9999999999999999999999", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("one", ParseStatusCode::kMalformedTag);
  ErrorTest<T>(" 1 ", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("1,", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("{$X}", ParseStatusCode::kMalformedTag);

  auto tag = OkTest<T>("0");
  EXPECT_EQ(getter_fn(tag), 0u);
  tag = OkTest<T>("1");
  EXPECT_EQ(getter_fn(tag), 1u);
  tag = OkTest<T>("10");
  EXPECT_EQ(getter_fn(tag), 10u);
  tag = OkTest<T>("14");
  EXPECT_EQ(getter_fn(tag), 14u);
  tag = OkTest<T>("999999999999999999");
  EXPECT_EQ(getter_fn(tag), 999999999999999999u);
}

VariableDictionary CreateBasicDictionary(
    const base::Location& from = base::Location::Current()) {
  VariableDictionary dict;
  EXPECT_TRUE(dict.Insert(CreateVarName("FOO"), "bar")) << from.ToString();
  EXPECT_TRUE(dict.Insert(CreateVarName("BAR"), "baz")) << from.ToString();

  return dict;
}

}  // namespace

TEST(HlsTagsTest, TagNameIdentity) {
  std::set<base::StringPiece> names;

  for (TagName name = kMinTagName; name <= kMaxTagName; ++name) {
    auto name_str = TagNameToString(name);

    // Name must be unique
    EXPECT_EQ(names.find(name_str), names.end());
    names.insert(name_str);

    // Name must parse to the original constant
    EXPECT_EQ(ParseTagName(name_str), name);
  }
}

TEST(HlsTagsTest, ParseM3uTag) {
  RunTagIdenficationTest<M3uTag>("#EXTM3U\n", absl::nullopt);
  RunEmptyTagTest<M3uTag>();
}

TEST(HlsTagsTest, ParseXVersionTag) {
  RunTagIdenficationTest<XVersionTag>("#EXT-X-VERSION:123\n", "123");

  // Test valid versions
  auto tag = OkTest<XVersionTag>("1");
  EXPECT_EQ(tag.version, 1u);
  tag = OkTest<XVersionTag>("2");
  EXPECT_EQ(tag.version, 2u);
  tag = OkTest<XVersionTag>("3");
  EXPECT_EQ(tag.version, 3u);
  tag = OkTest<XVersionTag>("4");
  EXPECT_EQ(tag.version, 4u);
  tag = OkTest<XVersionTag>("5");
  EXPECT_EQ(tag.version, 5u);
  tag = OkTest<XVersionTag>("6");
  EXPECT_EQ(tag.version, 6u);
  tag = OkTest<XVersionTag>("7");
  EXPECT_EQ(tag.version, 7u);
  tag = OkTest<XVersionTag>("8");
  EXPECT_EQ(tag.version, 8u);
  tag = OkTest<XVersionTag>("9");
  EXPECT_EQ(tag.version, 9u);
  tag = OkTest<XVersionTag>("10");
  EXPECT_EQ(tag.version, 10u);

  // While unsupported playlist versions are rejected, that's NOT the
  // responsibility of this tag parsing function. The playlist should be
  // rejected at a higher level.
  tag = OkTest<XVersionTag>("99999");
  EXPECT_EQ(tag.version, 99999u);

  // Test invalid versions
  ErrorTest<XVersionTag>(absl::nullopt, ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>("", ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>("0", ParseStatusCode::kInvalidPlaylistVersion);
  ErrorTest<XVersionTag>("-1", ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>("1.0", ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>("asdf", ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>("  1 ", ParseStatusCode::kMalformedTag);
}

TEST(HlsTagsTest, ParseInfTag) {
  RunTagIdenficationTest<InfTag>("#EXTINF:123,\t\n", "123,\t");

  // Test some valid tags
  auto tag = OkTest<InfTag>("1234,");
  EXPECT_EQ(tag.duration, 1234.0);
  EXPECT_EQ(tag.title.Str(), "");

  tag = OkTest<InfTag>("1.234,");
  EXPECT_EQ(tag.duration, 1.234);
  EXPECT_EQ(tag.title.Str(), "");

  // The spec implies that whitespace characters like this usually aren't
  // permitted, but "\t" is a common occurrence for the title value.
  tag = OkTest<InfTag>("99.5,\t");
  EXPECT_EQ(tag.duration, 99.5);
  EXPECT_EQ(tag.title.Str(), "\t");

  tag = OkTest<InfTag>("9.5,,,,");
  EXPECT_EQ(tag.duration, 9.5);
  EXPECT_EQ(tag.title.Str(), ",,,");

  tag = OkTest<InfTag>("12,asdfsdf   ");
  EXPECT_EQ(tag.duration, 12.0);
  EXPECT_EQ(tag.title.Str(), "asdfsdf   ");

  // Test some invalid tags
  ErrorTest<InfTag>(absl::nullopt, ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>("", ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>(",", ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>("-123,", ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>("123", ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>("asdf,", ParseStatusCode::kMalformedTag);
}

TEST(HlsTagsTest, ParseXIndependentSegmentsTag) {
  RunTagIdenficationTest<XIndependentSegmentsTag>(
      "#EXT-X-INDEPENDENT-SEGMENTS\n", absl::nullopt);
  RunEmptyTagTest<XIndependentSegmentsTag>();
}

TEST(HlsTagsTest, ParseXEndListTag) {
  RunTagIdenficationTest<XEndListTag>("#EXT-X-ENDLIST\n", absl::nullopt);
  RunEmptyTagTest<XEndListTag>();
}

TEST(HlsTagsTest, ParseXIFramesOnlyTag) {
  RunTagIdenficationTest<XIFramesOnlyTag>("#EXT-X-I-FRAMES-ONLY\n",
                                          absl::nullopt);
  RunEmptyTagTest<XIFramesOnlyTag>();
}

TEST(HlsTagsTest, ParseXDiscontinuityTag) {
  RunTagIdenficationTest<XDiscontinuityTag>("#EXT-X-DISCONTINUITY\n",
                                            absl::nullopt);
  RunEmptyTagTest<XDiscontinuityTag>();
}

TEST(HlsTagsTest, ParseXGapTag) {
  RunTagIdenficationTest<XGapTag>("#EXT-X-GAP\n", absl::nullopt);
  RunEmptyTagTest<XGapTag>();
}

TEST(HlsTagsTest, ParseXDefineTag) {
  RunTagIdenficationTest<XDefineTag>(
      "#EXT-X-DEFINE:NAME=\"FOO\",VALUE=\"Bar\",\n",
      "NAME=\"FOO\",VALUE=\"Bar\",");

  // Test some valid inputs
  auto tag = OkTest<XDefineTag>(R"(NAME="Foo",VALUE="bar",)");
  EXPECT_EQ(tag.name.GetName(), "Foo");
  EXPECT_TRUE(tag.value.has_value());
  EXPECT_EQ(tag.value.value(), "bar");

  tag = OkTest<XDefineTag>(R"(VALUE="90/12#%)(zx./",NAME="Hello12_-")");
  EXPECT_EQ(tag.name.GetName(), "Hello12_-");
  EXPECT_TRUE(tag.value.has_value());
  EXPECT_EQ(tag.value.value(), "90/12#%)(zx./");

  tag = OkTest<XDefineTag>(R"(IMPORT="-F90_Baz")");
  EXPECT_EQ(tag.name.GetName(), "-F90_Baz");
  EXPECT_FALSE(tag.value.has_value());

  // IMPORT and VALUE are not currently considered an error
  tag = OkTest<XDefineTag>(R"(IMPORT="F00_Bar",VALUE="Test")");
  EXPECT_EQ(tag.name.GetName(), "F00_Bar");
  EXPECT_FALSE(tag.value.has_value());

  // NAME with empty value is allowed
  tag = OkTest<XDefineTag>(R"(NAME="HELLO",VALUE="")");
  EXPECT_EQ(tag.name.GetName(), "HELLO");
  EXPECT_TRUE(tag.value.has_value());
  EXPECT_EQ(tag.value.value(), "");

  // Empty content is not allowed
  ErrorTest<XDefineTag>(absl::nullopt, ParseStatusCode::kMalformedTag);
  ErrorTest<XDefineTag>("", ParseStatusCode::kMalformedTag);

  // NAME and IMPORT are NOT allowed
  ErrorTest<XDefineTag>(R"(NAME="Foo",IMPORT="Foo")",
                        ParseStatusCode::kMalformedTag);

  // Name without VALUE is NOT allowed
  ErrorTest<XDefineTag>(R"(NAME="Foo",)", ParseStatusCode::kMalformedTag);

  // Empty NAME is not allowed
  ErrorTest<XDefineTag>(R"(NAME="",VALUE="Foo")",
                        ParseStatusCode::kMalformedTag);

  // Non-valid NAME is not allowed
  ErrorTest<XDefineTag>(R"(NAME=".FOO",VALUE="Foo")",
                        ParseStatusCode::kMalformedTag);
  ErrorTest<XDefineTag>(R"(NAME="F++OO",VALUE="Foo")",
                        ParseStatusCode::kMalformedTag);
  ErrorTest<XDefineTag>(R"(NAME=" FOO",VALUE="Foo")",
                        ParseStatusCode::kMalformedTag);
  ErrorTest<XDefineTag>(R"(NAME="FOO ",VALUE="Foo")",
                        ParseStatusCode::kMalformedTag);
}

TEST(HlsTagsTest, ParseXPlaylistTypeTag) {
  RunTagIdenficationTest<XPlaylistTypeTag>("#EXT-X-PLAYLIST-TYPE:VOD\n", "VOD");
  RunTagIdenficationTest<XPlaylistTypeTag>("#EXT-X-PLAYLIST-TYPE:EVENT\n",
                                           "EVENT");

  auto tag = OkTest<XPlaylistTypeTag>("EVENT");
  EXPECT_EQ(tag.type, PlaylistType::kEvent);
  tag = OkTest<XPlaylistTypeTag>("VOD");
  EXPECT_EQ(tag.type, PlaylistType::kVOD);

  ErrorTest<XPlaylistTypeTag>("FOOBAR", ParseStatusCode::kUnknownPlaylistType);
  ErrorTest<XPlaylistTypeTag>("EEVENT", ParseStatusCode::kUnknownPlaylistType);
  ErrorTest<XPlaylistTypeTag>(" EVENT", ParseStatusCode::kUnknownPlaylistType);
  ErrorTest<XPlaylistTypeTag>("EVENT ", ParseStatusCode::kUnknownPlaylistType);
  ErrorTest<XPlaylistTypeTag>("", ParseStatusCode::kMalformedTag);
  ErrorTest<XPlaylistTypeTag>(absl::nullopt, ParseStatusCode::kMalformedTag);
}

TEST(HlsTagsTest, ParseXStreamInfTag) {
  RunTagIdenficationTest<XStreamInfTag>(
      "#EXT-X-STREAM-INF:BANDWIDTH=1010,CODECS=\"foo,bar\"\n",
      "BANDWIDTH=1010,CODECS=\"foo,bar\"");

  VariableDictionary variable_dict = CreateBasicDictionary();
  VariableDictionary::SubstitutionBuffer sub_buffer;

  auto tag = OkTest<XStreamInfTag>(
      R"(BANDWIDTH=1010,AVERAGE-BANDWIDTH=1000,CODECS="foo,bar",SCORE=12.2)",
      variable_dict, sub_buffer);
  EXPECT_EQ(tag.bandwidth, 1010u);
  EXPECT_EQ(tag.average_bandwidth, 1000u);
  EXPECT_DOUBLE_EQ(tag.score.value(), 12.2);
  EXPECT_EQ(tag.codecs, "foo,bar");
  EXPECT_EQ(tag.resolution, absl::nullopt);
  EXPECT_EQ(tag.frame_rate, absl::nullopt);

  tag = OkTest<XStreamInfTag>(
      R"(BANDWIDTH=1010,RESOLUTION=1920x1080,FRAME-RATE=29.97)", variable_dict,
      sub_buffer);
  EXPECT_EQ(tag.bandwidth, 1010u);
  EXPECT_EQ(tag.average_bandwidth, absl::nullopt);
  EXPECT_EQ(tag.score, absl::nullopt);
  EXPECT_EQ(tag.codecs, absl::nullopt);
  ASSERT_TRUE(tag.resolution.has_value());
  EXPECT_EQ(tag.resolution->width, 1920u);
  EXPECT_EQ(tag.resolution->height, 1080u);
  EXPECT_DOUBLE_EQ(tag.frame_rate.value(), 29.97);

  // "BANDWIDTH" is the only required attribute
  tag = OkTest<XStreamInfTag>(R"(BANDWIDTH=5050)", variable_dict, sub_buffer);
  EXPECT_EQ(tag.bandwidth, 5050u);
  EXPECT_EQ(tag.average_bandwidth, absl::nullopt);
  EXPECT_EQ(tag.score, absl::nullopt);
  EXPECT_EQ(tag.codecs, absl::nullopt);
  EXPECT_EQ(tag.resolution, absl::nullopt);
  EXPECT_EQ(tag.frame_rate, absl::nullopt);

  ErrorTest<XStreamInfTag>(absl::nullopt, variable_dict, sub_buffer,
                           ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>("", variable_dict, sub_buffer,
                           ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(CODECS="foo,bar")", variable_dict, sub_buffer,
                           ParseStatusCode::kMalformedTag);

  // "BANDWIDTH" must be a valid DecimalInteger (non-negative)
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH="111")", variable_dict, sub_buffer,
                           ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=-1)", variable_dict, sub_buffer,
                           ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1.5)", variable_dict, sub_buffer,
                           ParseStatusCode::kMalformedTag);

  // "AVERAGE-BANDWIDTH" must be a valid DecimalInteger (non-negative)
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,AVERAGE-BANDWIDTH="111")",
                           variable_dict, sub_buffer,
                           ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,AVERAGE-BANDWIDTH=-1)",
                           variable_dict, sub_buffer,
                           ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,AVERAGE-BANDWIDTH=1.5)",
                           variable_dict, sub_buffer,
                           ParseStatusCode::kMalformedTag);

  // "SCORE" must be a valid DecimalFloatingPoint (non-negative)
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,SCORE="1")", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,SCORE=-1)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,SCORE=ONE)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);

  // "CODECS" must be a valid string
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,CODECS=abc,123)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,CODECS=abc)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,CODECS=123)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);

  // "CODECS" is subject to variable substitution
  tag = OkTest<XStreamInfTag>(R"(BANDWIDTH=1010,CODECS="{$FOO},{$BAR}")",
                              variable_dict, sub_buffer);
  EXPECT_EQ(tag.bandwidth, 1010u);
  EXPECT_EQ(tag.average_bandwidth, absl::nullopt);
  EXPECT_EQ(tag.score, absl::nullopt);
  EXPECT_EQ(tag.codecs, "bar,baz");
  EXPECT_EQ(tag.resolution, absl::nullopt);

  // "RESOLUTION" must be a valid decimal-resolution
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,RESOLUTION=1920x)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,RESOLUTION=x123)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);

  // "FRAME-RATE" must be a valid decimal-floating-point (unsigned)
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,FRAME-RATE=-1)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,FRAME-RATE=One)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,FRAME-RATE=30.0.0)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
}

TEST(HlsTagsTest, ParseXTargetDurationTag) {
  RunTagIdenficationTest<XTargetDurationTag>("#EXT-X-TARGETDURATION:10\n",
                                             "10");
  RunDecimalIntegerTagTest<XTargetDurationTag>(
      std::mem_fn(&XTargetDurationTag::duration));
}

TEST(HlsTagsTest, ParseXMediaSequenceTag) {
  RunTagIdenficationTest<XMediaSequenceTag>("#EXT-X-MEDIA-SEQUENCE:3\n", "3");
  RunDecimalIntegerTagTest<XMediaSequenceTag>(
      std::mem_fn(&XMediaSequenceTag::number));
}

}  // namespace media::hls
