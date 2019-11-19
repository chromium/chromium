// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TextResourceDecoderTest, UTF8Decode) {
  std::unique_ptr<TextResourceDecoder> decoder =
      std::make_unique<TextResourceDecoder>(
          TextResourceDecoderOptions::CreateUTF8Decode());
  const unsigned char kFooUTF8WithBOM[] = {0xef, 0xbb, 0xbf, 0x66, 0x6f, 0x6f};
  WTF::String decoded = decoder->Decode(
      reinterpret_cast<const char*>(kFooUTF8WithBOM), sizeof(kFooUTF8WithBOM));
  decoded = decoded + decoder->Flush();
  EXPECT_EQ(WTF::UTF8Encoding(), decoder->Encoding());
  EXPECT_EQ("foo", decoded);
}

TEST(TextResourceDecoderTest, UTF8DecodeWithoutBOM) {
  std::unique_ptr<TextResourceDecoder> decoder =
      std::make_unique<TextResourceDecoder>(
          TextResourceDecoderOptions::CreateUTF8DecodeWithoutBOM());
  const unsigned char kFooUTF8WithBOM[] = {0xef, 0xbb, 0xbf, 0x66, 0x6f, 0x6f};
  WTF::String decoded = decoder->Decode(
      reinterpret_cast<const char*>(kFooUTF8WithBOM), sizeof(kFooUTF8WithBOM));
  decoded = decoded + decoder->Flush();
  EXPECT_EQ(WTF::UTF8Encoding(), decoder->Encoding());
  EXPECT_EQ(
      "\xef\xbb\xbf"
      "foo",
      decoded.Utf8());
}

TEST(TextResourceDecoderTest, BasicUTF16) {
  std::unique_ptr<TextResourceDecoder> decoder =
      std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kPlainTextContent));
  WTF::String decoded;

  const unsigned char kFooLE[] = {0xff, 0xfe, 0x66, 0x00,
                                  0x6f, 0x00, 0x6f, 0x00};
  decoded =
      decoder->Decode(reinterpret_cast<const char*>(kFooLE), sizeof(kFooLE));
  decoded = decoded + decoder->Flush();
  EXPECT_EQ("foo", decoded);

  decoder = std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
      TextResourceDecoderOptions::kPlainTextContent));
  const unsigned char kFooBE[] = {0xfe, 0xff, 0x00, 0x66,
                                  0x00, 0x6f, 0x00, 0x6f};
  decoded =
      decoder->Decode(reinterpret_cast<const char*>(kFooBE), sizeof(kFooBE));
  decoded = decoded + decoder->Flush();
  EXPECT_EQ("foo", decoded);
}

TEST(TextResourceDecoderTest, UTF16Pieces) {
  std::unique_ptr<TextResourceDecoder> decoder =
      std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kPlainTextContent));

  WTF::String decoded;
  const unsigned char kFoo[] = {0xff, 0xfe, 0x66, 0x00, 0x6f, 0x00, 0x6f, 0x00};
  for (char c : kFoo)
    decoded = decoded + decoder->Decode(reinterpret_cast<const char*>(&c), 1);
  decoded = decoded + decoder->Flush();
  EXPECT_EQ("foo", decoded);
}

TEST(TextResourceDecoderTest, ContentSniffingStopsAfterSuccess) {
  std::unique_ptr<TextResourceDecoder> decoder =
      std::make_unique<TextResourceDecoder>(
          TextResourceDecoderOptions::CreateWithAutoDetection(
              TextResourceDecoderOptions::kPlainTextContent,
              WTF::UTF8Encoding(), WTF::UTF8Encoding(), KURL("")));

  std::string utf8_bytes =
      "tnegirjji gosa gii beare s\xC3\xA1htt\xC3\xA1 \xC4\x8D\xC3"
      "\xA1llit artihkkaliid. Maid don s\xC3\xA1ht\xC3\xA1t dievasmah";

  std::string eucjp_bytes =
      "<TITLE>"
      "\xA5\xD1\xA5\xEF\xA1\xBC\xA5\xC1\xA5\xE3\xA1\xBC\xA5\xC8\xA1\xC3\xC5\xEA"
      "\xBB\xF1\xBE\xF0\xCA\xF3\xA4\xCE\xA5\xD5\xA5\xA3\xA5\xB9\xA5\xB3</"
      "TITLE>";

  decoder->Decode(utf8_bytes.c_str(), utf8_bytes.length());
  EXPECT_EQ(WTF::UTF8Encoding(), decoder->Encoding());
  decoder->Decode(eucjp_bytes.c_str(), eucjp_bytes.length());
  EXPECT_EQ(WTF::UTF8Encoding(), decoder->Encoding());
}

}  // namespace blink
