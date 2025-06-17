// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_codec_icu.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(TextCodecIcuTest, IgnorableCodePoint) {
  TextEncoding iso2022jp("iso-2022-jp");
  std::unique_ptr<TextCodec> codec = TextCodecIcu::Create(iso2022jp, nullptr);
  Vector<UChar> source;
  source.push_back('a');
  source.push_back(kZeroWidthJoinerCharacter);
  std::string encoded = codec->Encode(
      base::span(source), UnencodableHandling::kEntitiesForUnencodables);
  EXPECT_EQ("a&#8205;", encoded);
  const String source2(u"ABC~¤•★星🌟星★•¤~XYZ");
  const std::string encoded2(codec->Encode(
      source2.Span16(), UnencodableHandling::kEntitiesForUnencodables));
  const String source3(u"ABC~&#164;&#8226;★星&#127775;星★&#8226;&#164;~XYZ");
  const std::string encoded3(codec->Encode(
      source3.Span16(), UnencodableHandling::kEntitiesForUnencodables));
  EXPECT_EQ(encoded3, encoded2);
  EXPECT_EQ(
      "ABC~&#164;&#8226;\x1B$B!z@1\x1B(B&#127775;\x1B$B@1!z\x1B(B&#8226;&#164;~"
      "XYZ",
      encoded2);
}

}  // namespace blink
