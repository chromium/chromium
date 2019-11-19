// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_codec_replacement.h"

#include <memory>

#include "base/stl_util.h"
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
  EXPECT_STREQ("replacement", TextEncoding(g_replacement_alias).GetName());
}

TEST(TextCodecReplacement, DecodesToFFFD) {
  TextEncoding encoding(g_replacement_alias);
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  bool saw_error = false;
  const char kTestCase[] = "hello world";
  wtf_size_t test_case_size = sizeof(kTestCase) - 1;

  const String result =
      codec->Decode(kTestCase, test_case_size, FlushBehavior::kDataEOF,
                    false, saw_error);
  EXPECT_TRUE(saw_error);
  ASSERT_EQ(1u, result.length());
  EXPECT_EQ(0xFFFDU, result[0]);
}

TEST(TextCodecReplacement, EncodesToUTF8) {
  TextEncoding encoding(g_replacement_alias);
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  // "Kanji" in Chinese characters.
  const UChar kTestCase[] = {0x6F22, 0x5B57};
  wtf_size_t test_case_size = base::size(kTestCase);
  std::string result =
      codec->Encode(kTestCase, test_case_size, kEntitiesForUnencodables);

  EXPECT_EQ("\xE6\xBC\xA2\xE5\xAD\x97", result);
}

}  // namespace

}  // namespace WTF
