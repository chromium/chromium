/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/text_codec_utf8.h"

#include <limits>
#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

namespace {

TEST(TextCodecUTF8, DecodeAscii) {
  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  bool saw_error = false;
  const auto input = base::byte_span_from_cstring("HelloWorld");
  const String& result =
      codec->Decode(input, FlushBehavior::kDataEOF, false, saw_error);
  EXPECT_FALSE(saw_error);
  ASSERT_EQ(input.size(), result.length());
  for (wtf_size_t i = 0; i < input.size(); ++i) {
    EXPECT_EQ(input[i], result[i]);
  }
}

TEST(TextCodecUTF8, DecodeChineseCharacters) {
  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  // "Kanji" in Chinese characters.
  const char kTestCase[] = "\xe6\xbc\xa2\xe5\xad\x97";

  bool saw_error = false;
  const String& result =
      codec->Decode(base::byte_span_from_cstring(kTestCase),
                    FlushBehavior::kDataEOF, false, saw_error);
  EXPECT_FALSE(saw_error);
  ASSERT_EQ(2u, result.length());
  EXPECT_EQ(0x6f22U, result[0]);
  EXPECT_EQ(0x5b57U, result[1]);
}

TEST(TextCodecUTF8, Decode0xFF) {
  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  bool saw_error = false;
  const String& result =
      codec->Decode(base::byte_span_from_cstring("\xff"),
                    FlushBehavior::kDataEOF, false, saw_error);
  EXPECT_TRUE(saw_error);
  ASSERT_EQ(1u, result.length());
  EXPECT_EQ(0xFFFDU, result[0]);
}

TEST(TextCodecUTF8, DecodeOverflow) {
  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  // Prime the partial sequence buffer.
  bool saw_error = false;
  codec->Decode(base::byte_span_from_cstring("\xC2"),
                FlushBehavior::kDoNotFlush, false, saw_error);
  EXPECT_FALSE(saw_error);

  EXPECT_DEATH_IF_SUPPORTED(
      codec->Decode(base::as_bytes(
                        base::span("", std::numeric_limits<wtf_size_t>::max())),
                    FlushBehavior::kDataEOF, false, saw_error),
      "");
}

}  // namespace

}  // namespace WTF
