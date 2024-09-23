// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/tags.h"

#include <array>
#include <optional>
#include <string_view>
#include <utility>

#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_serializers.h"
#include "media/formats/hls/items.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/test_util.h"
#include "media/formats/hls/variable_dictionary.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media::hls {

namespace {

// Returns the maximum whole quantity of seconds that can be represented by this
// implementation.
types::DecimalInteger MaxSeconds() {
  return base::TimeDelta::FiniteMax().InSeconds();
}

template <typename T>
void ErrorTest(std::optional<std::string_view> content,
               ParseStatusCode expected_status,
               const base::Location& from = base::Location::Current()) {
  auto tag = content ? TagItem::Create(ToTagName(T::kName),
                                       SourceString::CreateForTesting(*content))
                     : TagItem::CreateEmpty(ToTagName(T::kName), 1);
  auto result = T::Parse(tag);
  ASSERT_FALSE(result.has_value()) << from.ToString();
  auto error = std::move(result).error();
  EXPECT_EQ(error.code(), expected_status) << from.ToString();
}

template <typename T>
void ErrorTest(std::optional<std::string_view> content,
               const VariableDictionary& variable_dict,
               VariableDictionary::SubstitutionBuffer& sub_buffer,
               ParseStatusCode expected_status,
               const base::Location& from = base::Location::Current()) {
  auto tag = content ? TagItem::Create(ToTagName(T::kName),
                                       SourceString::CreateForTesting(*content))
                     : TagItem::CreateEmpty(ToTagName(T::kName), 1);
  auto result = T::Parse(tag, variable_dict, sub_buffer);
  ASSERT_FALSE(result.has_value()) << from.ToString();
  auto error = std::move(result).error();
  EXPECT_EQ(error.code(), expected_status)
      << "Actual Error: " << MediaSerialize(error) << "\n"
      << from.ToString();
}

template <typename T>
struct OkTestResult {
  T tag;

  // `tag` may have references to this string. To avoid UAF when moving
  // small strings we wrap it in a `std::unique_ptr`.
  std::unique_ptr<std::string> source;
};

template <typename T>
OkTestResult<T> OkTest(std::optional<std::string> content,
                       const base::Location& from = base::Location::Current()) {
  std::unique_ptr<std::string> source =
      content ? std::make_unique<std::string>(std::move(*content)) : nullptr;
  auto tag = source ? TagItem::Create(ToTagName(T::kName),
                                      SourceString::CreateForTesting(*source))
                    : TagItem::CreateEmpty(ToTagName(T::kName), 1);
  auto result = T::Parse(tag);
  EXPECT_TRUE(result.has_value()) << from.ToString();
  return OkTestResult<T>{.tag = std::move(result).value(),
                         .source = std::move(source)};
}

template <typename T>
OkTestResult<T> OkTest(std::optional<std::string> content,
                       const VariableDictionary& variable_dict,
                       VariableDictionary::SubstitutionBuffer& sub_buffer,
                       const base::Location& from = base::Location::Current()) {
  auto source =
      content ? std::make_unique<std::string>(std::move(*content)) : nullptr;
  auto tag = source ? TagItem::Create(ToTagName(T::kName),
                                      SourceString::CreateForTesting(*source))
                    : TagItem::CreateEmpty(ToTagName(T::kName), 1);
  auto result = T::Parse(tag, variable_dict, sub_buffer);
  if (!result.has_value()) {
    CHECK(false) << from.ToString() << "\n"
                 << MediaSerialize(std::move(result).error());
    NOTREACHED();
  }
  return OkTestResult<T>{.tag = std::move(result).value(),
                         .source = std::move(source)};
}

// Helper to test identification of this tag in a manifest.
// `line` must be a sample line containing this tag, and must end with a
// newline. This DOES NOT parse the item content (only that the item content
// matches what was expected), use `OkTest` and `ErrorTest` for that.
void RunTagIdenficationTest(
    TagName name,
    std::string_view line,
    std::optional<std::string_view> expected_content,
    const base::Location& from = base::Location::Current()) {
  auto iter = SourceLineIterator(line);
  auto item_result = GetNextLineItem(&iter);
  ASSERT_TRUE(item_result.has_value()) << from.ToString();

  auto item = std::move(item_result).value();
  auto* tag = absl::get_if<TagItem>(&item);
  ASSERT_NE(tag, nullptr) << from.ToString();
  EXPECT_EQ(tag->GetName(), name) << from.ToString();
  EXPECT_EQ(tag->GetContent().has_value(), expected_content.has_value())
      << from.ToString();
  if (tag->GetContent().has_value() && expected_content.has_value()) {
    EXPECT_EQ(tag->GetContent()->Str(), *expected_content) << from.ToString();
  }
}

template <typename T>
void RunTagIdenficationTest(
    std::string_view line,
    std::optional<std::string_view> expected_content,
    const base::Location& from = base::Location::Current()) {
  RunTagIdenficationTest(ToTagName(T::kName), line, expected_content, from);
}

// Test helper for tags which are expected to have no content
template <typename T>
void RunEmptyTagTest() {
  // Empty content is the only allowed content
  OkTest<T>(std::nullopt);

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
template <typename T>
void RunDecimalIntegerTagTest(types::DecimalInteger T::*field) {
  // Content is required
  ErrorTest<T>(std::nullopt, ParseStatusCode::kMalformedTag);
  ErrorTest<T>("", ParseStatusCode::kMalformedTag);

  // Content must be a valid decimal-integer
  ErrorTest<T>("-1", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("-1.5", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("-.5", ParseStatusCode::kMalformedTag);
  ErrorTest<T>(".5", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("0.5", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("one", ParseStatusCode::kMalformedTag);
  ErrorTest<T>(" 1 ", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("1,", ParseStatusCode::kMalformedTag);
  ErrorTest<T>("{$X}", ParseStatusCode::kMalformedTag);

  auto result = OkTest<T>("0");
  EXPECT_EQ(result.tag.*field, 0u);
  result = OkTest<T>("1");
  EXPECT_EQ(result.tag.*field, 1u);
  result = OkTest<T>("10");
  EXPECT_EQ(result.tag.*field, 10u);
  result = OkTest<T>("14");
  EXPECT_EQ(result.tag.*field, 14u);
}

VariableDictionary CreateBasicDictionary(
    const base::Location& from = base::Location::Current()) {
  VariableDictionary dict;
  EXPECT_TRUE(dict.Insert(CreateVarName("FOO"), "bar")) << from.ToString();
  EXPECT_TRUE(dict.Insert(CreateVarName("BAR"), "baz")) << from.ToString();
  EXPECT_TRUE(dict.Insert(CreateVarName("EMPTY"), "")) << from.ToString();
  EXPECT_TRUE(dict.Insert(CreateVarName("BAZ"), "foo")) << from.ToString();

  return dict;
}

}  // namespace

TEST(HlsTagsTest, TagNameIdentity) {
  std::set<std::string_view> names;

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
  RunTagIdenficationTest<M3uTag>("#EXTM3U\n", std::nullopt);
  RunEmptyTagTest<M3uTag>();
}

TEST(HlsTagsTest, ParseXDefineTag) {
  RunTagIdenficationTest<XDefineTag>(
      "#EXT-X-DEFINE:NAME=\"FOO\",VALUE=\"Bar\",\n",
      "NAME=\"FOO\",VALUE=\"Bar\",");

  // Test some valid inputs
  auto result = OkTest<XDefineTag>(R"(NAME="Foo",VALUE="bar",)");
  EXPECT_EQ(result.tag.name.GetName(), "Foo");
  EXPECT_TRUE(result.tag.value.has_value());
  EXPECT_EQ(result.tag.value.value(), "bar");

  result = OkTest<XDefineTag>(R"(VALUE="90/12#%)(zx./",NAME="Hello12_-")");
  EXPECT_EQ(result.tag.name.GetName(), "Hello12_-");
  EXPECT_TRUE(result.tag.value.has_value());
  EXPECT_EQ(result.tag.value.value(), "90/12#%)(zx./");

  result = OkTest<XDefineTag>(R"(IMPORT="-F90_Baz")");
  EXPECT_EQ(result.tag.name.GetName(), "-F90_Baz");
  EXPECT_FALSE(result.tag.value.has_value());

  // IMPORT and VALUE are not currently considered an error
  result = OkTest<XDefineTag>(R"(IMPORT="F00_Bar",VALUE="Test")");
  EXPECT_EQ(result.tag.name.GetName(), "F00_Bar");
  EXPECT_FALSE(result.tag.value.has_value());

  // NAME with empty value is allowed
  result = OkTest<XDefineTag>(R"(NAME="HELLO",VALUE="")");
  EXPECT_EQ(result.tag.name.GetName(), "HELLO");
  EXPECT_TRUE(result.tag.value.has_value());
  EXPECT_EQ(result.tag.value.value(), "");

  // Empty content is not allowed
  ErrorTest<XDefineTag>(std::nullopt, ParseStatusCode::kMalformedTag);
  ErrorTest<XDefineTag>("", ParseStatusCode::kMalformedTag);

  // NAME and IMPORT are NOT allowed
  ErrorTest<XDefineTag>(R"(NAME="Foo",IMPORT="Foo")",
                        ParseStatusCode::kMalformedTag);

  // Name without VALUE is NOT allowed
  ErrorTest<XDefineTag>(R"(NAME="Foo",)", ParseStatusCode::kMalformedTag);

  // Empty NAME is not allowed
  ErrorTest<XDefineTag>(R"(NAME="",VALUE="Foo")",
                        ParseStatusCode::kMalformedTag);

  // Empty IMPORT is not allowed
  ErrorTest<XDefineTag>(R"(IMPORT="")", ParseStatusCode::kMalformedTag);

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

TEST(HlsTagsTest, ParseXIndependentSegmentsTag) {
  RunTagIdenficationTest<XIndependentSegmentsTag>(
      "#EXT-X-INDEPENDENT-SEGMENTS\n", std::nullopt);
  RunEmptyTagTest<XIndependentSegmentsTag>();
}

TEST(HlsTagsTest, ParseXStartTag) {
  RunTagIdenficationTest(ToTagName(CommonTagName::kXStart),
                         "#EXT-X-START:TIME-OFFSET=30,PRECISE=YES\n",
                         "TIME-OFFSET=30,PRECISE=YES");
  // TODO(crbug.com/40057824): Implement the EXT-X-START tag.
}

TEST(HlsTagsTest, ParseXVersionTag) {
  RunTagIdenficationTest<XVersionTag>("#EXT-X-VERSION:123\n", "123");

  // Test valid versions
  auto result = OkTest<XVersionTag>("1");
  EXPECT_EQ(result.tag.version, 1u);
  result = OkTest<XVersionTag>("2");
  EXPECT_EQ(result.tag.version, 2u);
  result = OkTest<XVersionTag>("3");
  EXPECT_EQ(result.tag.version, 3u);
  result = OkTest<XVersionTag>("4");
  EXPECT_EQ(result.tag.version, 4u);
  result = OkTest<XVersionTag>("5");
  EXPECT_EQ(result.tag.version, 5u);
  result = OkTest<XVersionTag>("6");
  EXPECT_EQ(result.tag.version, 6u);
  result = OkTest<XVersionTag>("7");
  EXPECT_EQ(result.tag.version, 7u);
  result = OkTest<XVersionTag>("8");
  EXPECT_EQ(result.tag.version, 8u);
  result = OkTest<XVersionTag>("9");
  EXPECT_EQ(result.tag.version, 9u);
  result = OkTest<XVersionTag>("10");
  EXPECT_EQ(result.tag.version, 10u);

  // While unsupported playlist versions are rejected, that's NOT the
  // responsibility of this tag parsing function. The playlist should be
  // rejected at a higher level.
  result = OkTest<XVersionTag>("99999");
  EXPECT_EQ(result.tag.version, 99999u);

  // Test invalid versions
  ErrorTest<XVersionTag>(std::nullopt, ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>("", ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>("0", ParseStatusCode::kInvalidPlaylistVersion);
  ErrorTest<XVersionTag>("-1", ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>("1.0", ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>("asdf", ParseStatusCode::kMalformedTag);
  ErrorTest<XVersionTag>("  1 ", ParseStatusCode::kMalformedTag);
}

TEST(HlsTagsTest, ParseXContentSteeringTag) {
  RunTagIdenficationTest(
      ToTagName(MultivariantPlaylistTagName::kXContentSteering),
      "#EXT-X-CONTENT-STEERING:SERVER-URI=\"https://google.com/"
      "manifest.json\"\n",
      "SERVER-URI=\"https://google.com/manifest.json\"");
  // TODO(crbug.com/40057824): Implement the EXT-X-CONTENT-STEERING tag.
}

TEST(HlsTagsTest, ParseXIFrameStreamInfTag) {
  RunTagIdenficationTest(
      ToTagName(MultivariantPlaylistTagName::kXIFrameStreamInf),
      "#EXT-X-I-FRAME-STREAM-INF:URI=\"foo.m3u8\",BANDWIDTH=1000\n",
      "URI=\"foo.m3u8\",BANDWIDTH=1000");
  // TODO(crbug.com/40057824): Implement the EXT-X-I-FRAME-STREAM-INF tag.
}

TEST(HlsTagsTest, ParseXMediaTag) {
  RunTagIdenficationTest(
      ToTagName(MultivariantPlaylistTagName::kXMedia),
      "#EXT-X-MEDIA:TYPE=VIDEO,URI=\"foo.m3u8\",GROUP-ID=\"HD\",NAME=\"Foo "
      "HD\"\n",
      "TYPE=VIDEO,URI=\"foo.m3u8\",GROUP-ID=\"HD\",NAME=\"Foo HD\"");

  VariableDictionary variable_dict = CreateBasicDictionary();
  VariableDictionary::SubstitutionBuffer sub_buffer;
  EXPECT_TRUE(variable_dict.Insert(CreateVarName("TYPE"), "AUDIO"));
  EXPECT_TRUE(variable_dict.Insert(CreateVarName("YEAH"), "{$YES}"));
  EXPECT_TRUE(variable_dict.Insert(CreateVarName("SRVC"), "SERVICE"));

  ErrorTest<XMediaTag>(std::nullopt, variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("", variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("123", variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("Foobar", variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);

  // TYPE attribute is required
  ErrorTest<XMediaTag>("GROUP-ID=\"group\",NAME=\"name\"", variable_dict,
                       sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("TYPE=FAKE,GROUP-ID=\"group\",NAME=\"name\"",
                       variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("TYPE={$TYPE},GROUP-ID=\"group\",NAME=\"name\"",
                       variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);

  auto result = OkTest<XMediaTag>("TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\"",
                                  variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  result = OkTest<XMediaTag>("TYPE=VIDEO,GROUP-ID=\"group\",NAME=\"name\"",
                             variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kVideo);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The URI attribute is REQUIRED if TYPE=SUBTITLES
  ErrorTest<XMediaTag>("TYPE=SUBTITLES,GROUP-ID=\"group\",NAME=\"name\"",
                       variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);
  result = OkTest<XMediaTag>(
      "TYPE=SUBTITLES,GROUP-ID=\"group\",NAME=\"name\",URI=\"foo.m3u8\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kSubtitles);
  EXPECT_EQ(result.tag.uri->Str(), "foo.m3u8");
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The URI attribute MUST NOT be present if TYPE=CLOSED-CAPTIONS
  ErrorTest<XMediaTag>(
      "TYPE=CLOSED-CAPTIONS,GROUP-ID=\"group\",NAME=\"name\",INSTREAM-ID="
      "\"CC1\",URI=\"foo.m3u8\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);

  // The URI attribute must be a valid quoted-string
  ErrorTest<XMediaTag>("TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",URI=foo",
                       variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",URI=\"\"",
                       variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",URI=\"{$EMPTY}\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",URI=\"foo.m3u8\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri->Str(), "foo.m3u8");
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The URI attribute is subject to variable substitution
  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",URI=\"{$FOO}.m3u8\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri->Str(), "bar.m3u8");
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The GROUP-ID attribute is REQUIRED, and must be a valid quoted-string
  ErrorTest<XMediaTag>("TYPE=AUDIO,NAME=\"name\"", variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("TYPE=AUDIO,GROUP-ID=foo,NAME=\"name\"", variable_dict,
                       sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("TYPE=AUDIO,GROUP-ID=\"\",NAME=\"name\"", variable_dict,
                       sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("TYPE=AUDIO,GROUP-ID=\"{$EMPTY}\",NAME=\"name\"",
                       variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);

  // The GROUP-ID attribute is subject to variable substitution
  result = OkTest<XMediaTag>("TYPE=AUDIO,GROUP-ID=\"foo{$FOO}\",NAME=\"name\"",
                             variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "foobar");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The LANGUAGE attribute must be a valid quoted-string
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",LANGUAGE=foo", variable_dict,
      sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",LANGUAGE=\"\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",LANGUAGE=\"{$EMPTY}\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);

  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",LANGUAGE=\"en\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  ASSERT_TRUE(result.tag.language.has_value());
  EXPECT_EQ(result.tag.language->Str(), "en");
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The LANGUAGE attribute is subject to variable substitution
  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",LANGUAGE=\"{$FOO}\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  ASSERT_TRUE(result.tag.language.has_value());
  EXPECT_EQ(result.tag.language->Str(), "bar");
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The ASSOC-LANGUAGE attribute must be a valid quoted-string
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",ASSOC-LANGUAGE=foo",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",ASSOC-LANGUAGE=\"\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",ASSOC-LANGUAGE=\"{$EMPTY}\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);

  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",ASSOC-LANGUAGE=\"en\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  ASSERT_TRUE(result.tag.associated_language.has_value());
  EXPECT_EQ(result.tag.associated_language->Str(), "en");
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The ASSOC-LANGUAGE attribute is subject to variable substitution
  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",ASSOC-LANGUAGE=\"{$FOO}\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  ASSERT_TRUE(result.tag.associated_language.has_value());
  EXPECT_EQ(result.tag.associated_language->Str(), "bar");
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // LANGUAGE and ASSOC-LANGUAGE are not mutually exclusive
  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",LANGUAGE=\"foo\",ASSOC-"
      "LANGUAGE=\"bar\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  ASSERT_TRUE(result.tag.language.has_value());
  EXPECT_EQ(result.tag.language->Str(), "foo");
  ASSERT_TRUE(result.tag.associated_language.has_value());
  EXPECT_EQ(result.tag.associated_language->Str(), "bar");
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The NAME attribute is REQUIRED, and must be a valid quoted-string
  ErrorTest<XMediaTag>("TYPE=AUDIO,GROUP-ID=\"group\"", variable_dict,
                       sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("TYPE=AUDIO,NAME=foo,GROUP-ID=\"group\"", variable_dict,
                       sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("TYPE=AUDIO,NAME=\"\",GROUP-ID=\"group\"", variable_dict,
                       sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>("TYPE=AUDIO,NAME=\"{$EMPTY}\",GROUP-ID=\"group\"",
                       variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);

  // NAME is subject to variable substitution
  result = OkTest<XMediaTag>("TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"foo{$FOO}\"",
                             variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "foobar");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The STABLE-RENDITION-ID attribute must be a valid quoted-string containing
  // a valid StableId
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",STABLE-RENDITION-ID=FOO",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",STABLE-RENDITION-ID=\"\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",STABLE-RENDITION-ID=\"{$"
      "EMPTY}\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",STABLE-RENDITION-ID=\"*\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);

  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",STABLE-RENDITION-ID="
      "\"abcABC123+/=.-_\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  ASSERT_TRUE(result.tag.stable_rendition_id.has_value());
  EXPECT_EQ(result.tag.stable_rendition_id->Str(), "abcABC123+/=.-_");
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // STABLE-RENDITION-ID is subject to variable substitution
  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",STABLE-RENDITION-ID=\"foo{$"
      "FOO}\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  ASSERT_TRUE(result.tag.stable_rendition_id.has_value());
  EXPECT_EQ(result.tag.stable_rendition_id->Str(), "foobar");
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The DEFAULT attribute must equal 'YES' to evaluate as true. Other values
  // are ignored, and it is not subject to variable substitution.
  for (std::string x : {"FOO", "Y", "{$YEAH}", "NO"}) {
    result = OkTest<XMediaTag>(
        "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",DEFAULT=" + x,
        variable_dict, sub_buffer);
    EXPECT_EQ(result.tag.type, MediaType::kAudio);
    EXPECT_EQ(result.tag.uri, std::nullopt);
    EXPECT_EQ(result.tag.group_id.Str(), "group");
    EXPECT_EQ(result.tag.language, std::nullopt);
    EXPECT_EQ(result.tag.associated_language, std::nullopt);
    EXPECT_EQ(result.tag.name.Str(), "name");
    EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
    EXPECT_EQ(result.tag.is_default, false);
    EXPECT_EQ(result.tag.autoselect, false);
    EXPECT_EQ(result.tag.forced, false);
    EXPECT_EQ(result.tag.instream_id, std::nullopt);
    EXPECT_EQ(result.tag.characteristics.size(), 0u);
    EXPECT_EQ(result.tag.channels, std::nullopt);
  }

  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",DEFAULT=YES", variable_dict,
      sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, true);
  EXPECT_EQ(result.tag.autoselect, true);  // DEFAULT=YES implies AUTOSELECT=YES
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The AUTOSELECT attribute must equal 'YES' to evaluate as true. Other values
  // are ignored, and it is not subject to variable substitution.
  for (std::string x : {"FOO", "Y", "{$YEAH}", "NO"}) {
    result = OkTest<XMediaTag>(
        "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",AUTOSELECT=" + x,
        variable_dict, sub_buffer);
    EXPECT_EQ(result.tag.type, MediaType::kAudio);
    EXPECT_EQ(result.tag.uri, std::nullopt);
    EXPECT_EQ(result.tag.group_id.Str(), "group");
    EXPECT_EQ(result.tag.language, std::nullopt);
    EXPECT_EQ(result.tag.associated_language, std::nullopt);
    EXPECT_EQ(result.tag.name.Str(), "name");
    EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
    EXPECT_EQ(result.tag.is_default, false);
    EXPECT_EQ(result.tag.autoselect, false);
    EXPECT_EQ(result.tag.forced, false);
    EXPECT_EQ(result.tag.instream_id, std::nullopt);
    EXPECT_EQ(result.tag.characteristics.size(), 0u);
    EXPECT_EQ(result.tag.channels, std::nullopt);
  }

  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",AUTOSELECT=YES",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, true);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // If DEFAULT=YES, then AUTOSELECT must be YES if present
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",DEFAULT=YES,AUTOSELECT=NO",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",DEFAULT=YES,AUTOSELECT=YES",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, true);
  EXPECT_EQ(result.tag.autoselect, true);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The FORCED attribute may only appear when TYPE=SUBTITLES
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,URI=\"foo.m3u8\",GROUP-ID=\"group\",NAME=\"name\",FORCED=YES",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,URI=\"foo.m3u8\",GROUP-ID=\"group\",NAME=\"name\",FORCED=NO",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);

  // The FORCED attribute must equal 'YES' to evaluate as true. Other values are
  // ignored, and it is not subject to variable substitution.
  for (std::string x : {"FOO", "Y", "{$YEAH}", "NO"}) {
    result = OkTest<XMediaTag>(
        "TYPE=SUBTITLES,URI=\"foo.m3u8\",GROUP-ID=\"group\",NAME=\"name\","
        "FORCED=" +
            x,
        variable_dict, sub_buffer);
    EXPECT_EQ(result.tag.type, MediaType::kSubtitles);
    ASSERT_TRUE(result.tag.uri.has_value());
    EXPECT_EQ(result.tag.uri->Str(), "foo.m3u8");
    EXPECT_EQ(result.tag.group_id.Str(), "group");
    EXPECT_EQ(result.tag.language, std::nullopt);
    EXPECT_EQ(result.tag.associated_language, std::nullopt);
    EXPECT_EQ(result.tag.name.Str(), "name");
    EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
    EXPECT_EQ(result.tag.is_default, false);
    EXPECT_EQ(result.tag.autoselect, false);
    EXPECT_EQ(result.tag.forced, false);
    EXPECT_EQ(result.tag.instream_id, std::nullopt);
    EXPECT_EQ(result.tag.characteristics.size(), 0u);
    EXPECT_EQ(result.tag.channels, std::nullopt);
  }

  result = OkTest<XMediaTag>(
      "TYPE=SUBTITLES,URI=\"foo.m3u8\",GROUP-ID=\"group\",NAME=\"name\",FORCED="
      "YES",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kSubtitles);
  ASSERT_TRUE(result.tag.uri.has_value());
  EXPECT_EQ(result.tag.uri->Str(), "foo.m3u8");
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, true);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // INSTREAM-ID is REQUIRED when TYPE=CLOSED-CAPTIONS, and MUST NOT appear for
  // any other type
  for (std::string x : {"AUDIO", "VIDEO", "SUBTITLES"}) {
    ErrorTest<XMediaTag>("TYPE=" + x +
                             ",URI=\"foo.m3u8\",GROUP-ID=\"group\",NAME="
                             "\"name\",INSTREAM-ID=\"CC1\"",
                         variable_dict, sub_buffer,
                         ParseStatusCode::kMalformedTag);
  }

  ErrorTest<XMediaTag>("TYPE=CLOSED-CAPTIONS,GROUP-ID=\"group\",NAME=\"name\"",
                       variable_dict, sub_buffer,
                       ParseStatusCode::kMalformedTag);
  result = OkTest<XMediaTag>(
      "TYPE=CLOSED-CAPTIONS,GROUP-ID=\"group\",NAME=\"name\",INSTREAM-ID="
      "\"CC1\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kClosedCaptions);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  ASSERT_TRUE(result.tag.instream_id.has_value());
  EXPECT_EQ(result.tag.instream_id->GetType(), types::InstreamId::Type::kCc);
  EXPECT_EQ(result.tag.instream_id->GetNumber(), 1);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // INSTREAM-ID must be a valid quoted-string containing an InstreamId, and is
  // subject to variable substitution.
  ErrorTest<XMediaTag>(
      "TYPE=CLOSED-CAPTIONS,GROUP-ID=\"group\",NAME=\"name\"INSTREAM-ID=CC1",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=CLOSED-CAPTIONS,GROUP-ID=\"group\",NAME=\"name\"INSTREAM-ID="
      "\"FOO\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=CLOSED-CAPTIONS,GROUP-ID=\"group\",NAME=\"name\"INSTREAM-ID="
      "\"SERVICE99\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  result = OkTest<XMediaTag>(
      "TYPE=CLOSED-CAPTIONS,GROUP-ID=\"group\",NAME=\"name\",INSTREAM-ID=\"{$"
      "SRVC}32\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kClosedCaptions);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  ASSERT_TRUE(result.tag.instream_id.has_value());
  EXPECT_EQ(result.tag.instream_id->GetType(),
            types::InstreamId::Type::kService);
  EXPECT_EQ(result.tag.instream_id->GetNumber(), 32);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The CHARACTERISTICS attribute must be a quoted-string containing a sequence
  // of media characteristics tags It may not be empty, and is subject to
  // variable substitution.
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHARACTERISTICS=foo",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHARACTERISTICS=\"\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);

  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHARACTERISTICS=\"foo,bar,"
      "baz\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 3u);
  EXPECT_EQ(result.tag.characteristics[0], "foo");
  EXPECT_EQ(result.tag.characteristics[1], "bar");
  EXPECT_EQ(result.tag.characteristics[2], "baz");
  EXPECT_EQ(result.tag.channels, std::nullopt);

  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHARACTERISTICS=\"{$FOO},{$"
      "BAR}\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 2u);
  EXPECT_EQ(result.tag.characteristics[0], "bar");
  EXPECT_EQ(result.tag.characteristics[1], "baz");
  EXPECT_EQ(result.tag.channels, std::nullopt);

  // The CHANNELS tag must be a non-empty quoted-string, and is subject to
  // variable substitution. The only parameters that are recognized are for
  // audio.
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHANNELS=foo", variable_dict,
      sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHANNELS=\"\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHANNELS=\"foo\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHANNELS=\"1/foo\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHANNELS=\"1/FOO,,BAR\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHANNELS=\"1/{$FOO}\"",
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);

  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHANNELS=\"1\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  ASSERT_TRUE(result.tag.channels.has_value());
  EXPECT_EQ(result.tag.channels->GetMaxChannels(), 1u);
  EXPECT_TRUE(result.tag.channels->GetAudioCodingIdentifiers().empty());

  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHANNELS=\"1/FOO,BAR,-\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  ASSERT_TRUE(result.tag.channels.has_value());
  EXPECT_EQ(result.tag.channels->GetMaxChannels(), 1u);
  EXPECT_EQ(result.tag.channels->GetAudioCodingIdentifiers().size(), 3u);
  EXPECT_EQ(result.tag.channels->GetAudioCodingIdentifiers()[0], "FOO");
  EXPECT_EQ(result.tag.channels->GetAudioCodingIdentifiers()[1], "BAR");
  EXPECT_EQ(result.tag.channels->GetAudioCodingIdentifiers()[2], "-");

  result = OkTest<XMediaTag>(
      "TYPE=AUDIO,GROUP-ID=\"group\",NAME=\"name\",CHANNELS=\"1/"
      "FOO,{$SRVC},-\"",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.type, MediaType::kAudio);
  EXPECT_EQ(result.tag.uri, std::nullopt);
  EXPECT_EQ(result.tag.group_id.Str(), "group");
  EXPECT_EQ(result.tag.language, std::nullopt);
  EXPECT_EQ(result.tag.associated_language, std::nullopt);
  EXPECT_EQ(result.tag.name.Str(), "name");
  EXPECT_EQ(result.tag.stable_rendition_id, std::nullopt);
  EXPECT_EQ(result.tag.is_default, false);
  EXPECT_EQ(result.tag.autoselect, false);
  EXPECT_EQ(result.tag.forced, false);
  EXPECT_EQ(result.tag.instream_id, std::nullopt);
  EXPECT_EQ(result.tag.characteristics.size(), 0u);
  EXPECT_TRUE(result.tag.channels.has_value());
  EXPECT_EQ(result.tag.channels->GetMaxChannels(), 1u);
  EXPECT_EQ(result.tag.channels->GetAudioCodingIdentifiers().size(), 3u);
  EXPECT_EQ(result.tag.channels->GetAudioCodingIdentifiers()[0], "FOO");
  EXPECT_EQ(result.tag.channels->GetAudioCodingIdentifiers()[1], "SERVICE");
  EXPECT_EQ(result.tag.channels->GetAudioCodingIdentifiers()[2], "-");
}

TEST(HlsTagsTest, ParseXSessionDataTag) {
  RunTagIdenficationTest(
      ToTagName(MultivariantPlaylistTagName::kXSessionData),
      "#EXT-X-SESSION-DATA:DATA-ID=\"com.google.key\",VALUE=\"value\"\n",
      "DATA-ID=\"com.google.key\",VALUE=\"value\"");
  // TODO(crbug.com/40057824): Implement the EXT-X-SESSION-DATA tag.
}

TEST(HlsTagsTest, ParseXSessionKeyTag) {
  RunTagIdenficationTest<XSessionKeyTag>("#EXT-X-SESSION-KEY:METHOD=NONE\n",
                                         "METHOD=NONE");

  VariableDictionary dict = CreateBasicDictionary();
  VariableDictionary::SubstitutionBuffer subs;

  // Invalid method
  ErrorTest<XSessionKeyTag>("METHOD=77", dict, subs,
                            ParseStatusCode::kMalformedTag);
  ErrorTest<XSessionKeyTag>("METHOD=NONE", dict, subs,
                            ParseStatusCode::kMalformedTag);

  // No IV when method is SAMPLE-AES-CTR
  ErrorTest<XSessionKeyTag>(
      "METHOD=SAMPLE-AES-CTR,IV=0xf4d52cf0dc02329c3ad6578744590658", dict, subs,
      ParseStatusCode::kMalformedTag);

  // Invalid IV
  ErrorTest<XSessionKeyTag>(
      "METHOD=AES-128,IV=0xf4d52cf0dc2329c3ad6578744590658", dict, subs,
      ParseStatusCode::kMalformedTag);

  {
    auto result = OkTest<XSessionKeyTag>(
        "METHOD=AES-128,URI=\"https://example.com\","
        "IV=0xf4d52cf0dc02329c3ad6578744590658",
        dict, subs);
    EXPECT_EQ(result.tag.method, XKeyTagMethod::kAES128);
    EXPECT_EQ(result.tag.uri.Str(), "https://example.com");
    EXPECT_TRUE(result.tag.iv.has_value());
    EXPECT_EQ(std::get<0>(result.tag.iv.value()), 0xf4d52cf0dc02329cu);
    EXPECT_EQ(std::get<1>(result.tag.iv.value()), 0x3ad6578744590658u);
    EXPECT_EQ(result.tag.keyformat, XKeyTagKeyFormat::kIdentity);
    EXPECT_FALSE(result.tag.keyformat_versions.has_value());
  }
  {
    auto result = OkTest<XSessionKeyTag>(
        "METHOD=AES-128,URI=\"https://example.com\","
        "KEYFORMAT=\"supersecretcrypto\"",
        dict, subs);
    EXPECT_EQ(result.tag.method, XKeyTagMethod::kAES128);
    EXPECT_EQ(result.tag.uri.Str(), "https://example.com");
    EXPECT_FALSE(result.tag.iv.has_value());
    EXPECT_EQ(result.tag.keyformat, XKeyTagKeyFormat::kUnsupported);
    EXPECT_FALSE(result.tag.keyformat_versions.has_value());
  }
  {
    auto result = OkTest<XSessionKeyTag>(
        "METHOD=SAMPLE-AES,URI=\"https://example.com\","
        "KEYFORMAT=\"supersecretcrypto\"",
        dict, subs);
    EXPECT_EQ(result.tag.method, XKeyTagMethod::kSampleAES);
    EXPECT_EQ(result.tag.uri.Str(), "https://example.com");
    EXPECT_FALSE(result.tag.iv.has_value());
    EXPECT_EQ(result.tag.keyformat, XKeyTagKeyFormat::kUnsupported);
    EXPECT_FALSE(result.tag.keyformat_versions.has_value());
  }
  {
    auto result = OkTest<XSessionKeyTag>(
        "METHOD=SAMPLE-AES-CTR,URI=\"https://example.com\","
        "KEYFORMAT=\"supersecretcrypto\"",
        dict, subs);
    EXPECT_EQ(result.tag.method, XKeyTagMethod::kSampleAESCTR);
    EXPECT_EQ(result.tag.uri.Str(), "https://example.com");
    EXPECT_FALSE(result.tag.iv.has_value());
    EXPECT_EQ(result.tag.keyformat, XKeyTagKeyFormat::kUnsupported);
    EXPECT_FALSE(result.tag.keyformat_versions.has_value());
  }
}

TEST(HlsTagsTest, ParseXStreamInfTag) {
  RunTagIdenficationTest<XStreamInfTag>(
      "#EXT-X-STREAM-INF:BANDWIDTH=1010,CODECS=\"foo,bar\"\n",
      "BANDWIDTH=1010,CODECS=\"foo,bar\"");

  VariableDictionary variable_dict = CreateBasicDictionary();
  VariableDictionary::SubstitutionBuffer sub_buffer;

  auto result = OkTest<XStreamInfTag>(
      R"(BANDWIDTH=1010,AVERAGE-BANDWIDTH=1000,CODECS="foo,bar",SCORE=12.2)",
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.bandwidth, 1010u);
  EXPECT_EQ(result.tag.average_bandwidth, 1000u);
  EXPECT_DOUBLE_EQ(result.tag.score.value(), 12.2);
  ASSERT_TRUE(result.tag.codecs.has_value());
  EXPECT_TRUE(
      base::ranges::equal(result.tag.codecs.value(), std::array{"foo", "bar"}));
  EXPECT_EQ(result.tag.resolution, std::nullopt);
  EXPECT_EQ(result.tag.frame_rate, std::nullopt);

  result = OkTest<XStreamInfTag>(
      R"(BANDWIDTH=1010,RESOLUTION=1920x1080,FRAME-RATE=29.97)", variable_dict,
      sub_buffer);
  EXPECT_EQ(result.tag.bandwidth, 1010u);
  EXPECT_EQ(result.tag.average_bandwidth, std::nullopt);
  EXPECT_EQ(result.tag.score, std::nullopt);
  EXPECT_EQ(result.tag.codecs, std::nullopt);
  ASSERT_TRUE(result.tag.resolution.has_value());
  EXPECT_EQ(result.tag.resolution->width, 1920u);
  EXPECT_EQ(result.tag.resolution->height, 1080u);
  EXPECT_DOUBLE_EQ(result.tag.frame_rate.value(), 29.97);

  // "BANDWIDTH" is the only required attribute
  result =
      OkTest<XStreamInfTag>(R"(BANDWIDTH=5050)", variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.bandwidth, 5050u);
  EXPECT_EQ(result.tag.average_bandwidth, std::nullopt);
  EXPECT_EQ(result.tag.score, std::nullopt);
  EXPECT_EQ(result.tag.codecs, std::nullopt);
  EXPECT_EQ(result.tag.resolution, std::nullopt);
  EXPECT_EQ(result.tag.frame_rate, std::nullopt);

  ErrorTest<XStreamInfTag>(std::nullopt, variable_dict, sub_buffer,
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
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,CODECS="")", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);

  // "CODECS" is subject to variable substitution
  result = OkTest<XStreamInfTag>(R"(BANDWIDTH=1010,CODECS="{$FOO},{$BAR}")",
                                 variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.bandwidth, 1010u);
  EXPECT_EQ(result.tag.average_bandwidth, std::nullopt);
  EXPECT_EQ(result.tag.score, std::nullopt);
  ASSERT_TRUE(result.tag.codecs.has_value());
  EXPECT_TRUE(
      base::ranges::equal(result.tag.codecs.value(), std::array{"bar", "baz"}));
  EXPECT_EQ(result.tag.resolution, std::nullopt);

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

  // "AUDIO" must be a valid quoted-string
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,AUDIO=1)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,AUDIO="")", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,AUDIO=stereo)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);

  // "AUDIO" is subject to variable substitution
  result = OkTest<XStreamInfTag>(R"(BANDWIDTH=1010,AUDIO="{$FOO}{$BAR}")",
                                 variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.bandwidth, 1010u);
  EXPECT_EQ(result.tag.average_bandwidth, std::nullopt);
  EXPECT_EQ(result.tag.score, std::nullopt);
  EXPECT_EQ(result.tag.codecs, std::nullopt);
  EXPECT_EQ(result.tag.resolution, std::nullopt);
  ASSERT_TRUE(result.tag.audio.has_value());
  ASSERT_FALSE(result.tag.video.has_value());
  EXPECT_EQ(result.tag.audio->Str(), "barbaz");

  // "VIDEO" must be a valid quoted-string
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,VIDEO=1)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,VIDEO="")", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XStreamInfTag>(R"(BANDWIDTH=1010,VIDEO=stereo)", variable_dict,
                           sub_buffer, ParseStatusCode::kMalformedTag);

  // "VIDEO" is subject to variable substitution
  result = OkTest<XStreamInfTag>(R"(BANDWIDTH=1010,VIDEO="{$BAZ}{$FOO}")",
                                 variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.bandwidth, 1010u);
  EXPECT_EQ(result.tag.average_bandwidth, std::nullopt);
  EXPECT_EQ(result.tag.score, std::nullopt);
  EXPECT_EQ(result.tag.codecs, std::nullopt);
  EXPECT_EQ(result.tag.resolution, std::nullopt);
  ASSERT_TRUE(result.tag.video.has_value());
  ASSERT_FALSE(result.tag.audio.has_value());
  EXPECT_EQ(result.tag.video->Str(), "foobar");
}

TEST(HlsTagsTest, ParseInfTag) {
  RunTagIdenficationTest<InfTag>("#EXTINF:123,\t\n", "123,\t");

  // Test some valid tags
  auto result = OkTest<InfTag>("123,");
  EXPECT_TRUE(RoughlyEqual(result.tag.duration, base::Seconds(123.0)));
  EXPECT_EQ(result.tag.title.Str(), "");

  result = OkTest<InfTag>("1.23,");
  EXPECT_TRUE(RoughlyEqual(result.tag.duration, base::Seconds(1.23)));
  EXPECT_EQ(result.tag.title.Str(), "");

  // The spec implies that whitespace characters like this usually aren't
  // permitted, but "\t" is a common occurrence for the title value.
  result = OkTest<InfTag>("99.5,\t");
  EXPECT_TRUE(RoughlyEqual(result.tag.duration, base::Seconds(99.5)));
  EXPECT_EQ(result.tag.title.Str(), "\t");

  result = OkTest<InfTag>("9.5,,,,");
  EXPECT_TRUE(RoughlyEqual(result.tag.duration, base::Seconds(9.5)));
  EXPECT_EQ(result.tag.title.Str(), ",,,");

  result = OkTest<InfTag>("12,asdfsdf   ");
  EXPECT_TRUE(RoughlyEqual(result.tag.duration, base::Seconds(12.0)));
  EXPECT_EQ(result.tag.title.Str(), "asdfsdf   ");

  // By Spec, this should be an error, but alas, feral manifests exists and
  // often lack the trailing comma emblematic of their domesticated brethren.
  // ErrorTest<InfTag>("123", ParseStatusCode::kMalformedTag);
  result = OkTest<InfTag>("123");
  EXPECT_TRUE(RoughlyEqual(result.tag.duration, base::Seconds(123)));
  EXPECT_EQ(result.tag.title.Str(), "");

  // Test some invalid tags
  ErrorTest<InfTag>(std::nullopt, ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>("", ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>(",", ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>("-123,", ParseStatusCode::kMalformedTag);
  ErrorTest<InfTag>("asdf,", ParseStatusCode::kMalformedTag);

  // Test max value
  result = OkTest<InfTag>(base::NumberToString(MaxSeconds()) + ",\t");
  EXPECT_TRUE(RoughlyEqual(result.tag.duration, base::Seconds(MaxSeconds())));
  ErrorTest<InfTag>(base::NumberToString(MaxSeconds() + 1) + ",\t",
                    ParseStatusCode::kValueOverflowsTimeDelta);
}

TEST(HlsTagsTest, ParseXBitrateTag) {
  RunTagIdenficationTest<XBitrateTag>("#EXT-X-BITRATE:3\n", "3");
  RunDecimalIntegerTagTest(&XBitrateTag::bitrate);
}

TEST(HlsTagsTest, ParseXByteRangeTag) {
  RunTagIdenficationTest<XByteRangeTag>("#EXT-X-BYTERANGE:12@34\n", "12@34");

  auto result = OkTest<XByteRangeTag>("12");
  EXPECT_EQ(result.tag.range.length, 12u);
  EXPECT_EQ(result.tag.range.offset, std::nullopt);
  result = OkTest<XByteRangeTag>("12@34");
  EXPECT_EQ(result.tag.range.length, 12u);
  EXPECT_EQ(result.tag.range.offset, 34u);

  ErrorTest<XByteRangeTag>("FOOBAR", ParseStatusCode::kMalformedTag);
  ErrorTest<XByteRangeTag>("12@", ParseStatusCode::kMalformedTag);
  ErrorTest<XByteRangeTag>("@34", ParseStatusCode::kMalformedTag);
  ErrorTest<XByteRangeTag>("@", ParseStatusCode::kMalformedTag);
  ErrorTest<XByteRangeTag>(" 12@34", ParseStatusCode::kMalformedTag);
  ErrorTest<XByteRangeTag>("12@34 ", ParseStatusCode::kMalformedTag);
  ErrorTest<XByteRangeTag>("", ParseStatusCode::kMalformedTag);
  ErrorTest<XByteRangeTag>(std::nullopt, ParseStatusCode::kMalformedTag);
}

TEST(HlsTagsTest, ParseXDateRangeTag) {
  RunTagIdenficationTest(
      ToTagName(MediaPlaylistTagName::kXDateRange),
      "#EXT-X-DATERANGE:ID=\"ad\",START-DATE=\"2022-07-19T01:04:57+0000\"\n",
      "ID=\"ad\",START-DATE=\"2022-07-19T01:04:57+0000\"");
  // TODO(crbug.com/40057824): Implement the EXT-X-DATERANGE tag.
}

TEST(HlsTagsTest, ParseXDiscontinuityTag) {
  RunTagIdenficationTest<XDiscontinuityTag>("#EXT-X-DISCONTINUITY\n",
                                            std::nullopt);
  RunEmptyTagTest<XDiscontinuityTag>();
}

TEST(HlsTagsTest, ParseXDiscontinuitySequenceTag) {
  RunTagIdenficationTest<XDiscontinuitySequenceTag>(
      "#EXT-X-DISCONTINUITY-SEQUENCE:3\n", "3");
  RunDecimalIntegerTagTest(&XDiscontinuitySequenceTag::number);
}

TEST(HlsTagsTest, ParseXEndListTag) {
  RunTagIdenficationTest<XEndListTag>("#EXT-X-ENDLIST\n", std::nullopt);
  RunEmptyTagTest<XEndListTag>();
}

TEST(HlsTagsTest, ParseXGapTag) {
  RunTagIdenficationTest<XGapTag>("#EXT-X-GAP\n", std::nullopt);
  RunEmptyTagTest<XGapTag>();
}

TEST(HlsTagsTest, ParseXIFramesOnlyTag) {
  RunTagIdenficationTest<XIFramesOnlyTag>("#EXT-X-I-FRAMES-ONLY\n",
                                          std::nullopt);
  RunEmptyTagTest<XIFramesOnlyTag>();
}

TEST(HlsTagsTest, ParseXKeyTag) {
  RunTagIdenficationTest<XKeyTag>("#EXT-X-KEY:METHOD=NONE\n", "METHOD=NONE");

  VariableDictionary dict = CreateBasicDictionary();
  VariableDictionary::SubstitutionBuffer subs;

  // Invalid method
  ErrorTest<XKeyTag>("METHOD=77", dict, subs, ParseStatusCode::kMalformedTag);

  // If method is NONE, other attributes MUST NOT be present.
  ErrorTest<XKeyTag>("METHOD=NONE,URI=\"https://example.com\"", dict, subs,
                     ParseStatusCode::kMalformedTag);
  ErrorTest<XKeyTag>("METHOD=NONE,IV=0xf4d52cf0dc02329c3ad6578744590658", dict,
                     subs, ParseStatusCode::kMalformedTag);
  ErrorTest<XKeyTag>("METHOD=NONE,KEYFORMAT=identity", dict, subs,
                     ParseStatusCode::kMalformedTag);
  ErrorTest<XKeyTag>("METHOD=NONE,KEYFORMATVERSIONS=1/2/3", dict, subs,
                     ParseStatusCode::kMalformedTag);

  // No IV when method is SAMPLE-AES-CTR
  ErrorTest<XKeyTag>(
      "METHOD=SAMPLE-AES-CTR,IV=0xf4d52cf0dc02329c3ad6578744590658", dict, subs,
      ParseStatusCode::kMalformedTag);

  // Invalid IV
  ErrorTest<XKeyTag>("METHOD=AES-128,IV=0xf4d52cf0dc2329c3ad6578744590658",
                     dict, subs, ParseStatusCode::kMalformedTag);

  // Not allowed certain methods with clearkey or widevine
  ErrorTest<XKeyTag>(
      "METHOD=AES-128,FORMAT=\"org.w3.clearkey\",IV="
      "0xf4d52cf0dc02329c3ad6578744590658",
      dict, subs, ParseStatusCode::kMalformedTag);
  // Not allowed certain methods with clearkey or widevine
  ErrorTest<XKeyTag>(
      "METHOD=AES-256,FORMAT=\"org.w3.clearkey\",IV="
      "0xf4d52cf0dc02329c3ad6578744590658",
      dict, subs, ParseStatusCode::kMalformedTag);

  {
    auto result = OkTest<XKeyTag>("METHOD=NONE", dict, subs);
    EXPECT_EQ(result.tag.method, XKeyTagMethod::kNone);
    EXPECT_FALSE(result.tag.uri.has_value());
    EXPECT_FALSE(result.tag.iv.has_value());
    EXPECT_EQ(result.tag.keyformat, XKeyTagKeyFormat::kIdentity);
    EXPECT_FALSE(result.tag.keyformat_versions.has_value());
  }
  {
    auto result = OkTest<XKeyTag>(
        "METHOD=AES-128,URI=\"https://example.com\","
        "IV=0xf4d52cf0dc02329c3ad6578744590658",
        dict, subs);
    EXPECT_EQ(result.tag.method, XKeyTagMethod::kAES128);
    EXPECT_TRUE(result.tag.uri.has_value());
    EXPECT_EQ(result.tag.uri.value().Str(), "https://example.com");
    EXPECT_TRUE(result.tag.iv.has_value());
    EXPECT_EQ(std::get<0>(result.tag.iv.value()), 0xf4d52cf0dc02329cu);
    EXPECT_EQ(std::get<1>(result.tag.iv.value()), 0x3ad6578744590658u);
    EXPECT_EQ(result.tag.keyformat, XKeyTagKeyFormat::kIdentity);
    EXPECT_FALSE(result.tag.keyformat_versions.has_value());
  }
  {
    auto result = OkTest<XKeyTag>(
        "METHOD=AES-128,URI=\"https://example.com\","
        "KEYFORMAT=\"supersecretcrypto\"",
        dict, subs);
    EXPECT_EQ(result.tag.method, XKeyTagMethod::kAES128);
    EXPECT_TRUE(result.tag.uri.has_value());
    EXPECT_EQ(result.tag.uri.value().Str(), "https://example.com");
    EXPECT_FALSE(result.tag.iv.has_value());
    EXPECT_EQ(result.tag.keyformat, XKeyTagKeyFormat::kUnsupported);
    EXPECT_FALSE(result.tag.keyformat_versions.has_value());
  }
  {
    auto result = OkTest<XKeyTag>(
        "METHOD=SAMPLE-AES,URI=\"https://example.com\","
        "KEYFORMAT=\"supersecretcrypto\"",
        dict, subs);
    EXPECT_EQ(result.tag.method, XKeyTagMethod::kSampleAES);
    EXPECT_TRUE(result.tag.uri.has_value());
    EXPECT_EQ(result.tag.uri.value().Str(), "https://example.com");
    EXPECT_FALSE(result.tag.iv.has_value());
    EXPECT_EQ(result.tag.keyformat, XKeyTagKeyFormat::kUnsupported);
    EXPECT_FALSE(result.tag.keyformat_versions.has_value());
  }
  {
    auto result = OkTest<XKeyTag>(
        "METHOD=SAMPLE-AES-CTR,URI=\"https://example.com\","
        "KEYFORMAT=\"supersecretcrypto\"",
        dict, subs);
    EXPECT_EQ(result.tag.method, XKeyTagMethod::kSampleAESCTR);
    EXPECT_TRUE(result.tag.uri.has_value());
    EXPECT_EQ(result.tag.uri.value().Str(), "https://example.com");
    EXPECT_FALSE(result.tag.iv.has_value());
    EXPECT_EQ(result.tag.keyformat, XKeyTagKeyFormat::kUnsupported);
    EXPECT_FALSE(result.tag.keyformat_versions.has_value());
  }
  {
    auto result = OkTest<XKeyTag>(
        "METHOD=SAMPLE-AES-CTR,URI=\"https://example.com\","
        "KEYFORMAT=\"org.w3.clearkey\"",
        dict, subs);
    EXPECT_EQ(result.tag.method, XKeyTagMethod::kSampleAESCTR);
    EXPECT_TRUE(result.tag.uri.has_value());
    EXPECT_EQ(result.tag.uri.value().Str(), "https://example.com");
    EXPECT_FALSE(result.tag.iv.has_value());
    EXPECT_EQ(result.tag.keyformat, XKeyTagKeyFormat::kClearKey);
    EXPECT_FALSE(result.tag.keyformat_versions.has_value());
  }
}

TEST(HlsTagsTest, ParseXMapTag) {
  RunTagIdenficationTest<XMapTag>("#EXT-X-MAP:URI=\"foo.ts\",BYTERANGE=12@0\n",
                                  "URI=\"foo.ts\",BYTERANGE=12@0");

  VariableDictionary variable_dict = CreateBasicDictionary();
  EXPECT_TRUE(variable_dict.Insert(CreateVarName("ONE"), "1"));
  EXPECT_TRUE(variable_dict.Insert(CreateVarName("TWO"), "2"));
  EXPECT_TRUE(variable_dict.Insert(CreateVarName("THREE"), "3"));
  VariableDictionary::SubstitutionBuffer sub_buffer;

  // The URI attribute is required
  ErrorTest<XMapTag>(std::nullopt, variable_dict, sub_buffer,
                     ParseStatusCode::kMalformedTag);
  ErrorTest<XMapTag>("", variable_dict, sub_buffer,
                     ParseStatusCode::kMalformedTag);
  ErrorTest<XMapTag>("BYTERANGE=12", variable_dict, sub_buffer,
                     ParseStatusCode::kMalformedTag);
  ErrorTest<XMapTag>("URI=foo.ts", variable_dict, sub_buffer,
                     ParseStatusCode::kMalformedTag);
  auto result =
      OkTest<XMapTag>("URI=\"foo.ts\",FUTURE=PROOF", variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.uri.Str(), "foo.ts");
  EXPECT_EQ(result.tag.byte_range, std::nullopt);

  // The URI attribute is subject to variable substitution
  ErrorTest<XMapTag>("URI=\"{$UNDEFINED}.ts\"", variable_dict, sub_buffer,
                     ParseStatusCode::kMalformedTag);
  result =
      OkTest<XMapTag>("URI=\"{$FOO}_{$BAR}.ts\"", variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.uri.Str(), "bar_baz.ts");
  EXPECT_EQ(result.tag.byte_range, std::nullopt);

  // Test the BYTERANGE attribute
  ErrorTest<XMapTag>("URI=\"foo.ts\",BYTERANGE=\"{$UNDEFINED}\"", variable_dict,
                     sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XMapTag>("URI=\"foo.ts\",BYTERANGE=\"\"", variable_dict, sub_buffer,
                     ParseStatusCode::kMalformedTag);
  result = OkTest<XMapTag>("URI=\"foo.ts\",BYTERANGE=\"10\"", variable_dict,
                           sub_buffer);
  EXPECT_EQ(result.tag.uri.Str(), "foo.ts");
  EXPECT_EQ(result.tag.byte_range->length, 10u);
  EXPECT_EQ(result.tag.byte_range->offset, std::nullopt);

  // The BYTERANGE attribute is subject to variable substitution
  result = OkTest<XMapTag>("URI=\"foo.ts\",BYTERANGE=\"{$ONE}{$TWO}@{$THREE}\"",
                           variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.uri.Str(), "foo.ts");
  EXPECT_EQ(result.tag.byte_range->length, 12u);
  EXPECT_EQ(result.tag.byte_range->offset, 3u);
}

TEST(HlsTagsTest, ParseXMediaSequenceTag) {
  RunTagIdenficationTest<XMediaSequenceTag>("#EXT-X-MEDIA-SEQUENCE:3\n", "3");
  RunDecimalIntegerTagTest(&XMediaSequenceTag::number);
}

TEST(HlsTagsTest, ParseXPartTag) {
  RunTagIdenficationTest<XPartTag>("#EXT-X-PART:URI=\"foo.ts\",DURATION=1\n",
                                   "URI=\"foo.ts\",DURATION=1");

  VariableDictionary variable_dict = CreateBasicDictionary();
  EXPECT_TRUE(variable_dict.Insert(CreateVarName("NUMBER"), "9"));
  VariableDictionary::SubstitutionBuffer sub_buffer;

  // The URI and DURATION attributes are required
  ErrorTest<XPartTag>(std::nullopt, variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);
  ErrorTest<XPartTag>("", variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);
  ErrorTest<XPartTag>("URI=\"foo.ts\"", variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);
  ErrorTest<XPartTag>("DURATION=1", variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);
  ErrorTest<XPartTag>("URI=\"\",DURATION=1", variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);
  auto result =
      OkTest<XPartTag>("URI=\"foo.ts\",DURATION=1", variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.uri.Str(), "foo.ts");
  EXPECT_EQ(result.tag.duration, base::Seconds(1));
  EXPECT_EQ(result.tag.byte_range, std::nullopt);
  EXPECT_EQ(result.tag.independent, false);
  EXPECT_EQ(result.tag.gap, false);

  // Test URI attribute
  ErrorTest<XPartTag>("URI=\"{$UNDEFINED}.ts\",DURATION=1", variable_dict,
                      sub_buffer, ParseStatusCode::kMalformedTag);
  result = OkTest<XPartTag>("URI=\"{$BAR}.ts\",DURATION=1", variable_dict,
                            sub_buffer);
  EXPECT_EQ(result.tag.uri.Str(), "baz.ts");
  EXPECT_EQ(result.tag.duration, base::Seconds(1));
  EXPECT_EQ(result.tag.byte_range, std::nullopt);
  EXPECT_EQ(result.tag.independent, false);
  EXPECT_EQ(result.tag.gap, false);

  // Test DURATION attribute
  result = OkTest<XPartTag>(
      "URI=\"foo.ts\",DURATION=" + base::NumberToString(MaxSeconds()),
      variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.uri.Str(), "foo.ts");
  EXPECT_TRUE(RoughlyEqual(result.tag.duration, base::Seconds(MaxSeconds())));
  EXPECT_EQ(result.tag.byte_range, std::nullopt);
  EXPECT_EQ(result.tag.independent, false);
  EXPECT_EQ(result.tag.gap, false);
  ErrorTest<XPartTag>(
      "URI=\"foo.ts\",DURATION=" + base::NumberToString(MaxSeconds() + 1),
      variable_dict, sub_buffer, ParseStatusCode::kMalformedTag);

  // Test BYTERANGE attribute
  ErrorTest<XPartTag>("URI=\"foo.ts\",DURATION=1,BYTERANGE=\"{$UNDEFINED}\"",
                      variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);
  ErrorTest<XPartTag>("URI=\"foo.ts\",DURATION=1,BYTERANGE=\"\"", variable_dict,
                      sub_buffer, ParseStatusCode::kMalformedTag);
  result = OkTest<XPartTag>("URI=\"foo.ts\",DURATION=1,BYTERANGE=\"12\"",
                            variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.uri.Str(), "foo.ts");
  EXPECT_EQ(result.tag.duration, base::Seconds(1));
  EXPECT_EQ(result.tag.byte_range->length, 12u);
  EXPECT_EQ(result.tag.byte_range->offset, std::nullopt);
  EXPECT_EQ(result.tag.independent, false);
  EXPECT_EQ(result.tag.gap, false);

  result =
      OkTest<XPartTag>("URI=\"foo.ts\",DURATION=1,BYTERANGE=\"{$NUMBER}@3\"",
                       variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.uri.Str(), "foo.ts");
  EXPECT_EQ(result.tag.duration, base::Seconds(1));
  EXPECT_EQ(result.tag.byte_range->length, 9u);
  EXPECT_EQ(result.tag.byte_range->offset, 3u);
  EXPECT_EQ(result.tag.independent, false);
  EXPECT_EQ(result.tag.gap, false);

  // Test the INDEPENDENT attribute
  result = OkTest<XPartTag>("URI=\"foo.ts\",DURATION=1,INDEPENDENT=YES",
                            variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.uri.Str(), "foo.ts");
  EXPECT_EQ(result.tag.duration, base::Seconds(1));
  EXPECT_EQ(result.tag.byte_range, std::nullopt);
  EXPECT_EQ(result.tag.independent, true);
  EXPECT_EQ(result.tag.gap, false);

  for (std::string x : {"NO", "Y", "TRUE", "1", "yes"}) {
    result = OkTest<XPartTag>("URI=\"foo.ts\",DURATION=1,INDEPENDENT=" + x,
                              variable_dict, sub_buffer);
    EXPECT_EQ(result.tag.uri.Str(), "foo.ts");
    EXPECT_EQ(result.tag.duration, base::Seconds(1));
    EXPECT_EQ(result.tag.byte_range, std::nullopt);
    EXPECT_EQ(result.tag.independent, false);
    EXPECT_EQ(result.tag.gap, false);
  }

  // Test the GAP attribute
  result = OkTest<XPartTag>("URI=\"foo.ts\",DURATION=1,GAP=YES", variable_dict,
                            sub_buffer);
  EXPECT_EQ(result.tag.uri.Str(), "foo.ts");
  EXPECT_EQ(result.tag.duration, base::Seconds(1));
  EXPECT_EQ(result.tag.byte_range, std::nullopt);
  EXPECT_EQ(result.tag.independent, false);
  EXPECT_EQ(result.tag.gap, true);

  for (std::string x : {"NO", "Y", "TRUE", "1", "yes"}) {
    result = OkTest<XPartTag>("URI=\"foo.ts\",DURATION=1,GAP=" + x,
                              variable_dict, sub_buffer);
    EXPECT_EQ(result.tag.uri.Str(), "foo.ts");
    EXPECT_EQ(result.tag.duration, base::Seconds(1));
    EXPECT_EQ(result.tag.byte_range, std::nullopt);
    EXPECT_EQ(result.tag.independent, false);
    EXPECT_EQ(result.tag.gap, false);
  }
}

TEST(HlsTagsTest, ParseXPartInfTag) {
  RunTagIdenficationTest<XPartInfTag>("#EXT-X-PART-INF:PART-TARGET=1.0\n",
                                      "PART-TARGET=1.0");

  // PART-TARGET is required, and must be a valid DecimalFloatingPoint
  ErrorTest<XPartInfTag>(std::nullopt, ParseStatusCode::kMalformedTag);
  ErrorTest<XPartInfTag>("", ParseStatusCode::kMalformedTag);
  ErrorTest<XPartInfTag>("1", ParseStatusCode::kMalformedTag);
  ErrorTest<XPartInfTag>("PART-TARGET=-1", ParseStatusCode::kMalformedTag);
  ErrorTest<XPartInfTag>("PART-TARGET={$part-target}",
                         ParseStatusCode::kMalformedTag);
  ErrorTest<XPartInfTag>("PART-TARGET=\"1\"", ParseStatusCode::kMalformedTag);
  ErrorTest<XPartInfTag>("PART-TARGET=one", ParseStatusCode::kMalformedTag);
  ErrorTest<XPartInfTag>("FOO=BAR", ParseStatusCode::kMalformedTag);
  ErrorTest<XPartInfTag>("PART-TARGET=10,PART-TARGET=10",
                         ParseStatusCode::kMalformedTag);

  auto result = OkTest<XPartInfTag>("PART-TARGET=1.2");
  EXPECT_TRUE(RoughlyEqual(result.tag.target_duration, base::Seconds(1.2)));
  result = OkTest<XPartInfTag>("PART-TARGET=1");
  EXPECT_TRUE(RoughlyEqual(result.tag.target_duration, base::Seconds(1)));
  result = OkTest<XPartInfTag>("PART-TARGET=0");
  EXPECT_TRUE(RoughlyEqual(result.tag.target_duration, base::Seconds(0)));
  result = OkTest<XPartInfTag>("FOO=BAR,PART-TARGET=100,BAR=BAZ");
  EXPECT_TRUE(RoughlyEqual(result.tag.target_duration, base::Seconds(100)));

  // Test the max value
  result =
      OkTest<XPartInfTag>("PART-TARGET=" + base::NumberToString(MaxSeconds()));
  EXPECT_TRUE(
      RoughlyEqual(result.tag.target_duration, base::Seconds(MaxSeconds())));
  ErrorTest<XPartInfTag>(
      "PART-TARGET=" + base::NumberToString(MaxSeconds() + 1),
      ParseStatusCode::kValueOverflowsTimeDelta);
}

TEST(HlsTagsTest, ParseXPlaylistTypeTag) {
  RunTagIdenficationTest<XPlaylistTypeTag>("#EXT-X-PLAYLIST-TYPE:VOD\n", "VOD");
  RunTagIdenficationTest<XPlaylistTypeTag>("#EXT-X-PLAYLIST-TYPE:EVENT\n",
                                           "EVENT");

  auto result = OkTest<XPlaylistTypeTag>("EVENT");
  EXPECT_EQ(result.tag.type, PlaylistType::kEvent);
  result = OkTest<XPlaylistTypeTag>("VOD");
  EXPECT_EQ(result.tag.type, PlaylistType::kVOD);

  ErrorTest<XPlaylistTypeTag>("FOOBAR", ParseStatusCode::kUnknownPlaylistType);
  ErrorTest<XPlaylistTypeTag>("EEVENT", ParseStatusCode::kUnknownPlaylistType);
  ErrorTest<XPlaylistTypeTag>(" EVENT", ParseStatusCode::kUnknownPlaylistType);
  ErrorTest<XPlaylistTypeTag>("EVENT ", ParseStatusCode::kUnknownPlaylistType);
  ErrorTest<XPlaylistTypeTag>("", ParseStatusCode::kMalformedTag);
  ErrorTest<XPlaylistTypeTag>(std::nullopt, ParseStatusCode::kMalformedTag);
}

TEST(HlsTagsTest, ParseXPreloadHintTag) {
  RunTagIdenficationTest(ToTagName(MediaPlaylistTagName::kXPreloadHint),
                         "#EXT-X-PRELOAD-HINT:TYPE=PART,URI=\"foo.ts\"\n",
                         "TYPE=PART,URI=\"foo.ts\"");
  // TODO(crbug.com/40057824): Implement the EXT-X-PRELOAD-HINT tag.
}

TEST(HlsTagsTest, ParseXProgramDateTimeTag) {
  RunTagIdenficationTest(
      ToTagName(MediaPlaylistTagName::kXProgramDateTime),
      "#EXT-X-PROGRAM-DATE-TIME:2010-02-19T14:54:23.031+08:00\n",
      "2010-02-19T14:54:23.031+08:00");

  ErrorTest<XProgramDateTimeTag>(std::nullopt, ParseStatusCode::kMalformedTag);
  ErrorTest<XProgramDateTimeTag>("", ParseStatusCode::kMalformedTag);
  ErrorTest<XProgramDateTimeTag>("today", ParseStatusCode::kMalformedTag);

  auto result = OkTest<XProgramDateTimeTag>("2010-02-19T14:54:23.031+08:00");
  EXPECT_EQ(result.tag.time.InMillisecondsSinceUnixEpoch(), 1266562463031);

  result = OkTest<XProgramDateTimeTag>("1970-01-01T00:00:00.1+00:00");
  EXPECT_EQ(result.tag.time.InMillisecondsSinceUnixEpoch(), 100);
}

TEST(HlsTagsTest, ParseXRenditionReportTag) {
  RunTagIdenficationTest(
      ToTagName(MediaPlaylistTagName::kXRenditionReport),
      "#EXT-X-RENDITION-REPORT:URI=\"foo.m3u8\",LAST-MSN=200\n",
      "URI=\"foo.m3u8\",LAST-MSN=200");

  VariableDictionary dict = CreateBasicDictionary();
  VariableDictionary::SubstitutionBuffer subs;

  ErrorTest<XRenditionReportTag>(std::nullopt, dict, subs,
                                 ParseStatusCode::kMalformedTag);
  ErrorTest<XRenditionReportTag>("URI", dict, subs,
                                 ParseStatusCode::kMalformedAttributeList);
  ErrorTest<XRenditionReportTag>("URI=\"", dict, subs,
                                 ParseStatusCode::kMalformedAttributeList);

  auto result = OkTest<XRenditionReportTag>("", dict, subs);
  EXPECT_FALSE(result.tag.uri.has_value());
  EXPECT_FALSE(result.tag.last_msn.has_value());
  EXPECT_FALSE(result.tag.last_part.has_value());

  result = OkTest<XRenditionReportTag>("URI=\"foo\"", dict, subs);
  EXPECT_EQ(result.tag.uri->Str(), "foo");
  EXPECT_FALSE(result.tag.last_msn.has_value());
  EXPECT_FALSE(result.tag.last_part.has_value());

  result = OkTest<XRenditionReportTag>("LAST-MSN=3,", dict, subs);
  EXPECT_FALSE(result.tag.uri.has_value());
  EXPECT_EQ(result.tag.last_msn.value(), 3u);
  EXPECT_FALSE(result.tag.last_part.has_value());

  result = OkTest<XRenditionReportTag>("LAST-PART=3", dict, subs);
  EXPECT_FALSE(result.tag.uri.has_value());
  EXPECT_FALSE(result.tag.last_msn.has_value());
  EXPECT_EQ(result.tag.last_part.value(), 3u);

  result = OkTest<XRenditionReportTag>("URI=\"x\",LAST-PART=3,LAST-MSN=1", dict,
                                       subs);
  EXPECT_EQ(result.tag.uri->Str(), "x");
  EXPECT_EQ(result.tag.last_msn.value(), 1u);
  EXPECT_EQ(result.tag.last_part.value(), 3u);
}

TEST(HlsTagsTest, ParseXServerControlTag) {
  RunTagIdenficationTest<XServerControlTag>(
      "#EXT-X-SERVER-CONTROL:SKIP-UNTIL=10\n", "SKIP-UNTIL=10");

  // Tag requires content
  ErrorTest<XServerControlTag>(std::nullopt, ParseStatusCode::kMalformedTag);

  // Content is allowed to be empty
  auto result = OkTest<XServerControlTag>("");
  EXPECT_EQ(result.tag.skip_boundary, std::nullopt);
  EXPECT_EQ(result.tag.can_skip_dateranges, false);
  EXPECT_EQ(result.tag.hold_back, std::nullopt);
  EXPECT_EQ(result.tag.part_hold_back, std::nullopt);
  EXPECT_EQ(result.tag.can_block_reload, false);

  result = OkTest<XServerControlTag>(
      "CAN-SKIP-UNTIL=50,CAN-SKIP-DATERANGES=YES,HOLD-BACK=60,PART-HOLD-BACK="
      "40,CAN-BLOCK-RELOAD=YES,FUTURE-PROOF=YES");
  EXPECT_TRUE(RoughlyEqual(result.tag.skip_boundary, base::Seconds(50)));
  EXPECT_EQ(result.tag.can_skip_dateranges, true);
  EXPECT_TRUE(RoughlyEqual(result.tag.hold_back, base::Seconds(60)));
  EXPECT_TRUE(RoughlyEqual(result.tag.part_hold_back, base::Seconds(40)));
  EXPECT_EQ(result.tag.can_block_reload, true);

  ErrorTest<XServerControlTag>("CAN-SKIP-UNTIL=-5",
                               ParseStatusCode::kMalformedTag);
  ErrorTest<XServerControlTag>("CAN-SKIP-UNTIL={$B}",
                               ParseStatusCode::kMalformedTag);
  ErrorTest<XServerControlTag>("CAN-SKIP-UNTIL=\"5\"",
                               ParseStatusCode::kMalformedTag);

  result = OkTest<XServerControlTag>("CAN-SKIP-UNTIL=5");
  EXPECT_TRUE(RoughlyEqual(result.tag.skip_boundary, base::Seconds(5)));
  EXPECT_EQ(result.tag.can_skip_dateranges, false);
  EXPECT_EQ(result.tag.hold_back, std::nullopt);
  EXPECT_EQ(result.tag.part_hold_back, std::nullopt);
  EXPECT_EQ(result.tag.can_block_reload, false);

  // Test max value
  result = OkTest<XServerControlTag>("CAN-SKIP-UNTIL=" +
                                     base::NumberToString(MaxSeconds()));
  EXPECT_TRUE(
      RoughlyEqual(result.tag.skip_boundary, base::Seconds(MaxSeconds())));
  EXPECT_EQ(result.tag.can_skip_dateranges, false);
  EXPECT_EQ(result.tag.hold_back, std::nullopt);
  EXPECT_EQ(result.tag.part_hold_back, std::nullopt);
  EXPECT_EQ(result.tag.can_block_reload, false);

  ErrorTest<XServerControlTag>(
      "CAN-SKIP-UNTIL=" + base::NumberToString(MaxSeconds() + 1),
      ParseStatusCode::kValueOverflowsTimeDelta);

  // 'CAN-SKIP-DATERANGES' requires the presence of 'CAN-SKIP-UNTIL'
  ErrorTest<XServerControlTag>("CAN-SKIP-DATERANGES=YES",
                               ParseStatusCode::kMalformedTag);
  result =
      OkTest<XServerControlTag>("CAN-SKIP-DATERANGES=YES,CAN-SKIP-UNTIL=50");
  EXPECT_TRUE(RoughlyEqual(result.tag.skip_boundary, base::Seconds(50)));
  EXPECT_EQ(result.tag.can_skip_dateranges, true);
  EXPECT_EQ(result.tag.hold_back, std::nullopt);
  EXPECT_EQ(result.tag.part_hold_back, std::nullopt);
  EXPECT_EQ(result.tag.can_block_reload, false);

  // The only value that results in `true` is "YES"
  for (std::string x : {"NO", "Y", "TRUE", "1", "yes"}) {
    result = OkTest<XServerControlTag>("CAN-SKIP-DATERANGES=" + x +
                                       ",CAN-SKIP-UNTIL=50");
    EXPECT_TRUE(RoughlyEqual(result.tag.skip_boundary, base::Seconds(50)));
    EXPECT_EQ(result.tag.can_skip_dateranges, false);
    EXPECT_EQ(result.tag.hold_back, std::nullopt);
    EXPECT_EQ(result.tag.part_hold_back, std::nullopt);
    EXPECT_EQ(result.tag.can_block_reload, false);
  }

  // 'HOLD-BACK' must be a valid DecimalFloatingPoint
  ErrorTest<XServerControlTag>("HOLD-BACK=-5", ParseStatusCode::kMalformedTag);
  ErrorTest<XServerControlTag>("HOLD-BACK={$B}",
                               ParseStatusCode::kMalformedTag);
  ErrorTest<XServerControlTag>("HOLD-BACK=\"5\"",
                               ParseStatusCode::kMalformedTag);

  result = OkTest<XServerControlTag>("HOLD-BACK=50");
  EXPECT_EQ(result.tag.skip_boundary, std::nullopt);
  EXPECT_EQ(result.tag.can_skip_dateranges, false);
  EXPECT_TRUE(RoughlyEqual(result.tag.hold_back, base::Seconds(50)));
  EXPECT_EQ(result.tag.part_hold_back, std::nullopt);
  EXPECT_EQ(result.tag.can_block_reload, false);

  // Test max value
  result = OkTest<XServerControlTag>("HOLD-BACK=" +
                                     base::NumberToString(MaxSeconds()));
  EXPECT_EQ(result.tag.skip_boundary, std::nullopt);
  EXPECT_EQ(result.tag.can_skip_dateranges, false);
  EXPECT_TRUE(RoughlyEqual(result.tag.hold_back, base::Seconds(MaxSeconds())));
  EXPECT_EQ(result.tag.part_hold_back, std::nullopt);
  EXPECT_EQ(result.tag.can_block_reload, false);
  ErrorTest<XServerControlTag>(
      "HOLD-BACK=" + base::NumberToString(MaxSeconds() + 1),
      ParseStatusCode::kValueOverflowsTimeDelta);

  // 'PART-HOLD-BACK' must be a valid DecimalFloatingPoint
  ErrorTest<XServerControlTag>("PART-HOLD-BACK=-5",
                               ParseStatusCode::kMalformedTag);
  ErrorTest<XServerControlTag>("PART-HOLD-BACK={$B}",
                               ParseStatusCode::kMalformedTag);
  ErrorTest<XServerControlTag>("PART-HOLD-BACK=\"5\"",
                               ParseStatusCode::kMalformedTag);

  result = OkTest<XServerControlTag>("PART-HOLD-BACK=50");
  EXPECT_EQ(result.tag.skip_boundary, std::nullopt);
  EXPECT_EQ(result.tag.can_skip_dateranges, false);
  EXPECT_EQ(result.tag.hold_back, std::nullopt);
  EXPECT_EQ(result.tag.part_hold_back, base::Seconds(50));
  EXPECT_EQ(result.tag.can_block_reload, false);

  // Test max value
  result = OkTest<XServerControlTag>("PART-HOLD-BACK=" +
                                     base::NumberToString(MaxSeconds()));
  EXPECT_EQ(result.tag.skip_boundary, std::nullopt);
  EXPECT_EQ(result.tag.can_skip_dateranges, false);
  EXPECT_EQ(result.tag.hold_back, std::nullopt);
  EXPECT_TRUE(
      RoughlyEqual(result.tag.part_hold_back, base::Seconds(MaxSeconds())));
  EXPECT_EQ(result.tag.can_block_reload, false);
  ErrorTest<XServerControlTag>(
      "PART-HOLD-BACK=" + base::NumberToString(MaxSeconds() + 1),
      ParseStatusCode::kValueOverflowsTimeDelta);

  // The only value that results in `true` is "YES"
  for (std::string x : {"NO", "Y", "TRUE", "1", "yes"}) {
    result = OkTest<XServerControlTag>("CAN-BLOCK-RELOAD=" + x);
    EXPECT_EQ(result.tag.skip_boundary, std::nullopt);
    EXPECT_EQ(result.tag.can_skip_dateranges, false);
    EXPECT_EQ(result.tag.hold_back, std::nullopt);
    EXPECT_EQ(result.tag.part_hold_back, std::nullopt);
    EXPECT_EQ(result.tag.can_block_reload, false);
  }

  result = OkTest<XServerControlTag>("CAN-BLOCK-RELOAD=YES");
  EXPECT_EQ(result.tag.skip_boundary, std::nullopt);
  EXPECT_EQ(result.tag.can_skip_dateranges, false);
  EXPECT_EQ(result.tag.hold_back, std::nullopt);
  EXPECT_EQ(result.tag.part_hold_back, std::nullopt);
  EXPECT_EQ(result.tag.can_block_reload, true);
}

TEST(HlsTagsTest, ParseXSkipTag) {
  RunTagIdenficationTest<XSkipTag>("#EXT-X-SKIP:SKIPPED-SEGMENTS=10\n",
                                   "SKIPPED-SEGMENTS=10");

  VariableDictionary variable_dict = CreateBasicDictionary();
  VariableDictionary::SubstitutionBuffer sub_buffer;

  ErrorTest<XSkipTag>(std::nullopt, variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);
  ErrorTest<XSkipTag>("-1", variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);
  ErrorTest<XSkipTag>("UNKNOWN=10", variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);
  ErrorTest<XSkipTag>("SKIPPED-SEGMENTS=f", variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);
  ErrorTest<XSkipTag>("RECENTLY-REMOVED-DATERANGES=\"\"", variable_dict,
                      sub_buffer, ParseStatusCode::kMalformedTag);
  ErrorTest<XSkipTag>("SKIPPED-SEGMENTS=1,RECENTLY-REMOVED-DATERANGES=hello",
                      variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);
  ErrorTest<XSkipTag>("SKIPPED-SEGMENTS=1,RECENTLY-REMOVED-DATERANGES=\"\t\"",
                      variable_dict, sub_buffer,
                      ParseStatusCode::kMalformedTag);

  auto result =
      OkTest<XSkipTag>("SKIPPED-SEGMENTS=10", variable_dict, sub_buffer);
  EXPECT_EQ(result.tag.skipped_segments, 10u);
  EXPECT_EQ(result.tag.recently_removed_dateranges, std::nullopt);

  auto with_empty_ranges =
      OkTest<XSkipTag>("SKIPPED-SEGMENTS=10,RECENTLY-REMOVED-DATERANGES=\"\"",
                       variable_dict, sub_buffer);
  EXPECT_EQ(with_empty_ranges.tag.skipped_segments, 10u);
  EXPECT_TRUE(with_empty_ranges.tag.recently_removed_dateranges.has_value());
  EXPECT_EQ(with_empty_ranges.tag.recently_removed_dateranges->size(), 0u);

  auto with_some_ranges = OkTest<XSkipTag>(
      "SKIPPED-SEGMENTS=10,RECENTLY-REMOVED-DATERANGES=\"F\tT\"", variable_dict,
      sub_buffer);
  EXPECT_EQ(with_some_ranges.tag.skipped_segments, 10u);
  EXPECT_TRUE(with_some_ranges.tag.recently_removed_dateranges.has_value());
  EXPECT_EQ(with_some_ranges.tag.recently_removed_dateranges->size(), 2u);
}

TEST(HlsTagsTest, ParseXTargetDurationTag) {
  RunTagIdenficationTest<XTargetDurationTag>("#EXT-X-TARGETDURATION:10\n",
                                             "10");

  // Content must be a valid decimal-integer
  ErrorTest<XTargetDurationTag>(std::nullopt, ParseStatusCode::kMalformedTag);
  ErrorTest<XTargetDurationTag>("", ParseStatusCode::kMalformedTag);
  ErrorTest<XTargetDurationTag>("-1", ParseStatusCode::kMalformedTag);
  ErrorTest<XTargetDurationTag>("1.5", ParseStatusCode::kMalformedTag);
  ErrorTest<XTargetDurationTag>(" 1", ParseStatusCode::kMalformedTag);
  ErrorTest<XTargetDurationTag>("1 ", ParseStatusCode::kMalformedTag);
  ErrorTest<XTargetDurationTag>("one", ParseStatusCode::kMalformedTag);
  ErrorTest<XTargetDurationTag>("{$ONE}", ParseStatusCode::kMalformedTag);
  ErrorTest<XTargetDurationTag>("1,", ParseStatusCode::kMalformedTag);

  auto result = OkTest<XTargetDurationTag>("0");
  EXPECT_TRUE(RoughlyEqual(result.tag.duration, base::Seconds(0)));

  result = OkTest<XTargetDurationTag>("1");
  EXPECT_TRUE(RoughlyEqual(result.tag.duration, base::Seconds(1)));

  result = OkTest<XTargetDurationTag>("99");
  EXPECT_TRUE(RoughlyEqual(result.tag.duration, base::Seconds(99)));

  // Test max value
  result = OkTest<XTargetDurationTag>(base::NumberToString(MaxSeconds()));
  EXPECT_EQ(result.tag.duration, base::Seconds(MaxSeconds()));
  ErrorTest<XTargetDurationTag>(base::NumberToString(MaxSeconds() + 1),
                                ParseStatusCode::kValueOverflowsTimeDelta);
}

}  // namespace media::hls
