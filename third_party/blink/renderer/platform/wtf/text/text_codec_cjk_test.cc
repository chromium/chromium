// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_codec_cjk.h"

#include <unicode/ucnv.h>
#include <unicode/utf16.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec_icu.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace blink {
namespace {

#if !defined(USING_SYSTEM_ICU)
TEST(TextCodecCjk, IcuCanOpenBig5Html) {
  UErrorCode error = U_ZERO_ERROR;
  UConverter* conv = ucnv_open("big5-html", &error);
  EXPECT_TRUE(U_SUCCESS(error)) << u_errorName(error);
  if (U_SUCCESS(error)) {
    const char input[] = {(char)0x88, (char)0x40};
    base::span<const char> input_span = input;
    const char* input_ptr = input_span.data();
    const char* input_limit = base::to_address(input_span.end());
    UChar output[10];
    base::span<UChar> output_span = output;
    UChar* output_ptr = output_span.data();
    UChar* output_limit = base::to_address(output_span.end());
    ucnv_toUnicode(conv, &output_ptr, output_limit, &input_ptr, input_limit,
                   nullptr, true, &error);
    EXPECT_TRUE(U_SUCCESS(error));
    // big5-html.ucm: <U31C0> \x88\x40 |3
    if (output_ptr != output_span.data()) {
      EXPECT_EQ(0x31C0, output[0]);
    }
    ucnv_close(conv);
  }
}
#endif

TEST(TextCodecCjk, Big5HkscsMapping) {
  TextEncoding big5("big5");
  std::unique_ptr<TextCodec> codec = NewTextCodec(big5);
  static const uint8_t kData[] = {0x9C, 0x71};
  bool saw_error = false;
  String result =
      codec->Decode(kData, FlushBehavior::kDataEof, false, saw_error);
  EXPECT_FALSE(saw_error);
  EXPECT_EQ(2u, result.length());  // Supplementary character is 2 UChars
  if (result.length() >= 2) {
    EXPECT_EQ(0x20021, result.CodePointAtOrZero(0));
  }
}

TEST(TextCodecCjk, IsSupported) {
  EXPECT_TRUE(TextCodecCjk::IsSupported("EUC-JP"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("Shift_JIS"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("EUC-KR"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("ISO-2022-JP"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("GBK"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("gb18030"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("Big5"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("big5"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("BIG5"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("Big5-HKSCS"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("big5-hkscs"));
  EXPECT_FALSE(TextCodecCjk::IsSupported("non-exist-encoding"));
}

TEST(TextCodecCjk, Big5DecodeInvalidSequences) {
  TextEncoding big5("big5");
  std::unique_ptr<TextCodec> codec = NewTextCodec(big5);

  {
    static const uint8_t kData[] = {0x81, 0x81};
    bool saw_error = false;
    String result =
        codec->Decode(kData, FlushBehavior::kDataEof, false, saw_error);
    EXPECT_EQ(1u, result.length())
        << "0x81 0x81 should produce 1 replacement character";
    if (result.length() > 0) {
      EXPECT_EQ(0xFFFD, result[0]);
    }
  }

  {
    static const uint8_t kData[] = {0x87, 0x87, 0x40};
    bool saw_error = false;
    String result =
        codec->Decode(kData, FlushBehavior::kDataEof, false, saw_error);
    EXPECT_EQ(2u, result.length());
    if (result.length() >= 2) {
      EXPECT_EQ(0xFFFD, result[0]);
      EXPECT_EQ('@', result[1]);
    }
  }

  {
    // A1 40 is U+3000 (IDEOGRAPHIC SPACE) in Big5.
    // This also verifies that the mapping table is correctly populated.
    static const uint8_t kData[] = {0xA1, 0x40};
    bool saw_error = false;
    String result =
        codec->Decode(kData, FlushBehavior::kDataEof, false, saw_error);
    EXPECT_FALSE(saw_error);
    EXPECT_EQ(1u, result.length());
    if (result.length() >= 1) {
      EXPECT_EQ(0x3000, result[0]);
    }
  }
}

}  // namespace
}  // namespace blink
