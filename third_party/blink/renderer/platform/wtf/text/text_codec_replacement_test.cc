// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_codec_replacement.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

namespace {

// Just one example, others are listed in the codec implementation.
const char* g_replacement_alias = "iso-2022-kr";

TEST(TextCodecReplacement, Aliases) {
  EXPECT_TRUE(TextEncoding("replacement").IsValid());
  EXPECT_TRUE(TextEncoding("rEpLaCeMeNt").IsValid());

  EXPECT_TRUE(TextEncoding(g_replacement_alias).IsValid());
  EXPECT_EQ("replacement", TextEncoding(g_replacement_alias).GetName());
}

TEST(TextCodecReplacement, DecodesToFFFD) {
  TextEncoding encoding(g_replacement_alias);
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  bool saw_error = false;

  const String result =
      codec->Decode(base::byte_span_from_cstring("hello world"),
                    FlushBehavior::kDataEOF, false, saw_error);
  EXPECT_TRUE(saw_error);
  ASSERT_EQ(1u, result.length());
  EXPECT_EQ(0xFFFDU, result[0]);
}

TEST(TextCodecReplacement, EncodesToUTF8) {
  TextEncoding encoding(g_replacement_alias);
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  // "Kanji" in Chinese characters.
  const UChar kTestCase[] = {0x6F22, 0x5B57};
  std::string result = codec->Encode(kTestCase, kEntitiesForUnencodables);

  EXPECT_EQ("\xE6\xBC\xA2\xE5\xAD\x97", result);
}

}  // namespace

}  // namespace WTF
