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

#include "third_party/blink/renderer/platform/wtf/text/text_codec_utf8.h"

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

namespace {

class TextCodecUtf8Test : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        base::features::kUtfConversionAsciiFastPath, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, TextCodecUtf8Test, testing::Bool());

TEST_P(TextCodecUtf8Test, DecodeAscii) {
  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  bool saw_error = false;
  const auto input = base::byte_span_from_cstring("HelloWorld");
  const String& result =
      codec->Decode(input, FlushBehavior::kDataEof, false, saw_error);
  EXPECT_FALSE(saw_error);
  ASSERT_EQ(input.size(), result.length());
  for (wtf_size_t i = 0; i < input.size(); ++i) {
    EXPECT_EQ(input[i], result[i]);
  }
}

TEST_P(TextCodecUtf8Test, DecodeChineseCharacters) {
  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  // "Kanji" in Chinese characters.
  const char kTestCase[] = "\xe6\xbc\xa2\xe5\xad\x97";

  bool saw_error = false;
  const String& result =
      codec->Decode(base::byte_span_from_cstring(kTestCase),
                    FlushBehavior::kDataEof, false, saw_error);
  EXPECT_FALSE(saw_error);
  ASSERT_EQ(2u, result.length());
  EXPECT_EQ(0x6f22U, result[0]);
  EXPECT_EQ(0x5b57U, result[1]);
}

TEST_P(TextCodecUtf8Test, Decode0xFF) {
  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  bool saw_error = false;
  const String& result =
      codec->Decode(base::byte_span_from_cstring("\xff"),
                    FlushBehavior::kDataEof, false, saw_error);
  EXPECT_TRUE(saw_error);
  ASSERT_EQ(1u, result.length());
  EXPECT_EQ(0xFFFDU, result[0]);
}

TEST_P(TextCodecUtf8Test, DecodeOverflow) {
  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  // Prime the partial sequence buffer.
  bool saw_error = false;
  codec->Decode(base::byte_span_from_cstring("\xC2"),
                FlushBehavior::kDoNotFlush, false, saw_error);
  EXPECT_FALSE(saw_error);

  EXPECT_DEATH_IF_SUPPORTED(
      // SAFETY: Unsafe operation for a death test.
      codec->Decode(base::as_bytes(UNSAFE_BUFFERS(base::span(
                        "", std::numeric_limits<wtf_size_t>::max()))),
                    FlushBehavior::kDataEof, false, saw_error),
      "");
}

TEST_P(TextCodecUtf8Test, DecodeMultiplePartialsAfterError) {
  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  constexpr std::array<uint8_t, 4> data = {0xf0, 0xc3, 0x80, 0x2a};
  const auto [first_chunk, second_chunk] = base::span(data).split_at(1u);

  bool saw_error = false;
  {
    String s = codec->Decode(first_chunk, FlushBehavior::kDoNotFlush, false,
                             saw_error);
    EXPECT_TRUE(s.empty());
    EXPECT_FALSE(saw_error);
  }
  {
    String s = codec->Decode(second_chunk, FlushBehavior::kDoNotFlush, false,
                             saw_error);
    ASSERT_EQ(s.length(), 3u);
    EXPECT_EQ(s[0], 0xfffd);  // 0xf0, invalid => replacement character
    EXPECT_EQ(s[1], 0x00c0);  // 0xc3 0x80
    EXPECT_EQ(s[2], 0x002a);  // 0x2a
    EXPECT_TRUE(saw_error);
  }
  {
    String s = codec->Decode({}, FlushBehavior::kDataEof, false, saw_error);
    EXPECT_TRUE(s.empty());
    EXPECT_TRUE(saw_error);
  }
}

void DecodeFastPathMatchesLegacy(std::string_view input, bool stop_on_error) {
  TextEncoding encoding("UTF-8");
  auto input_span = base::as_bytes(base::span(input));

  String out_fast;
  bool saw_error_fast = false;
  {
    base::test::ScopedFeatureList features(
        base::features::kUtfConversionAsciiFastPath);
    std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
    out_fast = codec->Decode(input_span, FlushBehavior::kDataEof, stop_on_error,
                             saw_error_fast);
  }

  String out_legacy;
  bool saw_error_legacy = false;
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(base::features::kUtfConversionAsciiFastPath);
    std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
    out_legacy = codec->Decode(input_span, FlushBehavior::kDataEof,
                               stop_on_error, saw_error_legacy);
  }

  EXPECT_EQ(saw_error_fast, saw_error_legacy);
  EXPECT_EQ(out_fast, out_legacy);
  EXPECT_EQ(out_fast.Is8Bit(), out_legacy.Is8Bit());
}
FUZZ_TEST(TextCodecUtf8FuzzTest, DecodeFastPathMatchesLegacy);

void DecodeStreamingFastPathMatchesLegacy(
    const std::vector<std::string>& chunks,
    bool stop_on_error) {
  TextEncoding encoding("UTF-8");

  std::vector<String> outputs_fast;
  std::vector<bool> errors_fast;
  {
    base::test::ScopedFeatureList features(
        base::features::kUtfConversionAsciiFastPath);
    std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
    for (size_t i = 0; i < chunks.size(); ++i) {
      bool saw_error = false;
      const bool is_last = (i + 1 == chunks.size());
      FlushBehavior flush =
          is_last ? FlushBehavior::kDataEof : FlushBehavior::kDoNotFlush;
      String out = codec->Decode(base::as_bytes(base::span(chunks[i])), flush,
                                 stop_on_error, saw_error);
      outputs_fast.push_back(out);
      errors_fast.push_back(saw_error);
    }
  }

  std::vector<String> outputs_legacy;
  std::vector<bool> errors_legacy;
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(base::features::kUtfConversionAsciiFastPath);
    std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
    for (size_t i = 0; i < chunks.size(); ++i) {
      bool saw_error = false;
      const bool is_last = (i + 1 == chunks.size());
      FlushBehavior flush =
          is_last ? FlushBehavior::kDataEof : FlushBehavior::kDoNotFlush;
      String out = codec->Decode(base::as_bytes(base::span(chunks[i])), flush,
                                 stop_on_error, saw_error);
      outputs_legacy.push_back(out);
      errors_legacy.push_back(saw_error);
    }
  }

  EXPECT_EQ(errors_fast, errors_legacy);
  EXPECT_EQ(outputs_fast, outputs_legacy);
  for (size_t i = 0; i < outputs_fast.size(); ++i) {
    EXPECT_EQ(outputs_fast[i].Is8Bit(), outputs_legacy[i].Is8Bit());
  }
}
FUZZ_TEST(TextCodecUtf8FuzzTest, DecodeStreamingFastPathMatchesLegacy);

void DecodeSimdutfFastPathMatchesLegacy(std::string_view input,
                                        bool stop_on_error) {
  TextEncoding encoding("UTF-8");
  auto input_span = base::as_bytes(base::span(input));

  String out_simdutf;
  bool saw_error_simdutf = false;
  {
    base::test::ScopedFeatureList features(
        features::kSimdutfTextCodecUtf8Decode);
    std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
    out_simdutf = codec->Decode(input_span, FlushBehavior::kDataEof,
                                stop_on_error, saw_error_simdutf);
  }

  String out_legacy;
  bool saw_error_legacy = false;
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(features::kSimdutfTextCodecUtf8Decode);
    std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
    out_legacy = codec->Decode(input_span, FlushBehavior::kDataEof,
                               stop_on_error, saw_error_legacy);
  }

  EXPECT_EQ(saw_error_simdutf, saw_error_legacy);
  EXPECT_EQ(out_simdutf, out_legacy);
  EXPECT_EQ(out_simdutf.Is8Bit(), out_legacy.Is8Bit());
}
FUZZ_TEST(TextCodecUtf8FuzzTest, DecodeSimdutfFastPathMatchesLegacy);

void DecodeStreamingSimdutfFastPathMatchesLegacy(
    const std::vector<std::string>& chunks,
    bool stop_on_error) {
  TextEncoding encoding("UTF-8");

  std::vector<String> outputs_simdutf;
  std::vector<bool> errors_simdutf;
  {
    base::test::ScopedFeatureList features(
        features::kSimdutfTextCodecUtf8Decode);
    std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
    for (size_t i = 0; i < chunks.size(); ++i) {
      bool saw_error = false;
      const bool is_last = (i + 1 == chunks.size());
      FlushBehavior flush =
          is_last ? FlushBehavior::kDataEof : FlushBehavior::kDoNotFlush;
      String out = codec->Decode(base::as_bytes(base::span(chunks[i])), flush,
                                 stop_on_error, saw_error);
      outputs_simdutf.push_back(out);
      errors_simdutf.push_back(saw_error);
    }
  }

  std::vector<String> outputs_legacy;
  std::vector<bool> errors_legacy;
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(features::kSimdutfTextCodecUtf8Decode);
    std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
    for (size_t i = 0; i < chunks.size(); ++i) {
      bool saw_error = false;
      const bool is_last = (i + 1 == chunks.size());
      FlushBehavior flush =
          is_last ? FlushBehavior::kDataEof : FlushBehavior::kDoNotFlush;
      String out = codec->Decode(base::as_bytes(base::span(chunks[i])), flush,
                                 stop_on_error, saw_error);
      outputs_legacy.push_back(out);
      errors_legacy.push_back(saw_error);
    }
  }

  EXPECT_EQ(errors_simdutf, errors_legacy);
  EXPECT_EQ(outputs_simdutf, outputs_legacy);
  for (size_t i = 0; i < outputs_simdutf.size(); ++i) {
    EXPECT_EQ(outputs_simdutf[i].Is8Bit(), outputs_legacy[i].Is8Bit());
  }
}
FUZZ_TEST(TextCodecUtf8FuzzTest, DecodeStreamingSimdutfFastPathMatchesLegacy);

struct DecodeOutcome {
  String output;
  bool saw_error = false;
};

DecodeOutcome DecodeWithSimdutfFlag(base::span<const uint8_t> input,
                                    FlushBehavior flush,
                                    bool stop_on_error,
                                    bool enable_simdutf) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureState(
      features::kSimdutfTextCodecUtf8Decode, enable_simdutf);
  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
  DecodeOutcome outcome;
  outcome.output =
      codec->Decode(input, flush, stop_on_error, outcome.saw_error);
  return outcome;
}

void ExpectSimdutfMatchesControl(std::string_view input,
                                 FlushBehavior flush = FlushBehavior::kDataEof,
                                 bool stop_on_error = false) {
  const auto bytes = base::as_bytes(base::span(input));
  const DecodeOutcome control =
      DecodeWithSimdutfFlag(bytes, flush, stop_on_error, false);
  const DecodeOutcome enabled =
      DecodeWithSimdutfFlag(bytes, flush, stop_on_error, true);
  EXPECT_EQ(enabled.saw_error, control.saw_error);
  EXPECT_EQ(enabled.output.Is8Bit(), control.output.Is8Bit());
  EXPECT_EQ(enabled.output, control.output);
}

TEST(SimdutfTextCodecUtf8Test, LargePureAsciiChunks) {
  // Exercise boundary sizes around 256 bytes and large (>64 KB) HTML-like
  // ASCII.
  for (size_t target_size :
       {size_t{63}, size_t{64}, size_t{255}, size_t{256}, size_t{257},
        size_t{512}, size_t{4096}, size_t{96 * 1024}}) {
    std::string ascii;
    ascii.reserve(target_size);
    constexpr std::string_view kSnippet =
        "<div class=\"item\"><a href=\"https://example.com/page?q=blink\">"
        "Section snippet text 0123456789</a></div>\n";
    while (ascii.size() + kSnippet.size() <= target_size) {
      ascii.append(kSnippet);
    }
    ascii.append(target_size - ascii.size(), 'x');

    const auto bytes = base::as_bytes(base::span(ascii));
    const DecodeOutcome control =
        DecodeWithSimdutfFlag(bytes, FlushBehavior::kDataEof, false, false);
    const DecodeOutcome enabled =
        DecodeWithSimdutfFlag(bytes, FlushBehavior::kDataEof, false, true);

    EXPECT_FALSE(control.saw_error);
    EXPECT_FALSE(enabled.saw_error);
    EXPECT_TRUE(enabled.output.Is8Bit());
    EXPECT_EQ(enabled.output.length(), ascii.size());
    EXPECT_EQ(enabled.output, control.output);
  }

  // Verify embedded NUL bytes in >=256-byte ASCII input are preserved.
  std::string with_nuls(512, 'a');
  with_nuls[0] = '\0';
  with_nuls[256] = '\0';
  with_nuls[511] = '\0';
  ExpectSimdutfMatchesControl(with_nuls);
}

TEST(SimdutfTextCodecUtf8Test, LargeAsciiAndLatin1Preserves8Bit) {
  std::string latin1_utf8;
  latin1_utf8.reserve(80 * 1024);
  // 32 KB ASCII prefix before the first Latin-1 sequence.
  latin1_utf8.append(32 * 1024, 'A');
  // Append Latin-1 code points U+0080..U+00FF encoded as 2-byte UTF-8
  // (0xC2 0x80 .. 0xC3 0xBF) interleaved with ASCII markup.
  for (int round = 0; round < 150; ++round) {
    for (uint32_t cp = 0x80; cp <= 0xFF; ++cp) {
      latin1_utf8.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      latin1_utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    latin1_utf8.append("<span>caf\xC3\xA9 &amp; \xC2\xA9 2026</span>");
  }
  ASSERT_GT(latin1_utf8.size(), 64u * 1024u);

  const auto bytes = base::as_bytes(base::span(latin1_utf8));
  const DecodeOutcome control =
      DecodeWithSimdutfFlag(bytes, FlushBehavior::kDataEof, false, false);
  const DecodeOutcome enabled =
      DecodeWithSimdutfFlag(bytes, FlushBehavior::kDataEof, false, true);

  EXPECT_FALSE(control.saw_error);
  EXPECT_FALSE(enabled.saw_error);
  EXPECT_TRUE(control.output.Is8Bit());
  EXPECT_TRUE(enabled.output.Is8Bit());
  EXPECT_EQ(enabled.output, control.output);

  // Also test when a Latin-1 sequence appears at index 0 (first_non_ascii ==
  // 0).
  std::string leading_latin1 = "\xC2\xA0" + std::string(70 * 1024, 'z');
  const DecodeOutcome leading_enabled =
      DecodeWithSimdutfFlag(base::as_bytes(base::span(leading_latin1)),
                            FlushBehavior::kDataEof, false, true);
  EXPECT_FALSE(leading_enabled.saw_error);
  EXPECT_TRUE(leading_enabled.output.Is8Bit());
  ExpectSimdutfMatchesControl(leading_latin1);
}

TEST(SimdutfTextCodecUtf8Test, LargeHtmlWithLateMultiByteUtf8) {
  // Case 1: >64 KB ASCII HTML prefix followed by late 3-byte (U+2082, U+2192)
  // and 4-byte astral (U+1F50D, U+1F600) UTF-8 sequences.
  std::string html;
  html.reserve(96 * 1024);
  while (html.size() < 72 * 1024) {
    html.append(
        "<div class=\"entry\"><h3>Document Section Heading</h3>"
        "<p>Standard ASCII paragraph text in streamed HTML body.</p></div>");
  }
  // U+2082 SUBSCRIPT TWO (\xE2\x82\x82), U+2192 RIGHTWARDS ARROW
  // (\xE2\x86\x92), U+1F50D LEFT-POINTING MAGNIFYING GLASS (\xF0\x9F\x94\x8D),
  // U+1F600 GRINNING FACE (\xF0\x9F\x98\x80).
  html.append(
      "<span>H\xE2\x82\x82O \xE2\x86\x92 \xF0\x9F\x94\x8D "
      "\xF0\x9F\x98\x80</span>");
  html.append(8 * 1024, ' ');
  ASSERT_GT(html.size(), 64u * 1024u);

  {
    const auto bytes = base::as_bytes(base::span(html));
    const DecodeOutcome control =
        DecodeWithSimdutfFlag(bytes, FlushBehavior::kDataEof, false, false);
    const DecodeOutcome enabled =
        DecodeWithSimdutfFlag(bytes, FlushBehavior::kDataEof, false, true);

    EXPECT_FALSE(control.saw_error);
    EXPECT_FALSE(enabled.saw_error);
    EXPECT_FALSE(control.output.Is8Bit());
    EXPECT_FALSE(enabled.output.Is8Bit());
    EXPECT_EQ(enabled.output, control.output);
  }

  // Case 2: Chunks where the first non-ASCII byte is Latin-1 (U+00A0 / U+00B7)
  // and a later character is >0xFF (2-byte 0xC4, 3-byte U+203A/U+2014, or
  // 4-byte emoji), at both <64-byte and >64-byte offsets into the suffix to
  // exercise both the 64-byte block loop and tail loop of
  // ContainsByteAboveLatin1Lead().
  for (size_t mid_ascii_len :
       {size_t{0}, size_t{17}, size_t{64}, size_t{200}, size_t{32 * 1024}}) {
    for (std::string_view utf16_tail :
         {std::string_view("\xC4\x80"),
          std::string_view("\xE2\x80\xBA\xE2\x80\x94"),
          std::string_view("\xF0\x9F\x98\x80")}) {
      std::string latin1_then_utf16(4096, 'a');
      latin1_then_utf16.append("\xC2\xA0\xC2\xB7\xC3\xA9");
      latin1_then_utf16.append(mid_ascii_len, 'b');
      latin1_then_utf16.append(utf16_tail);
      const auto bytes = base::as_bytes(base::span(latin1_then_utf16));
      const DecodeOutcome control =
          DecodeWithSimdutfFlag(bytes, FlushBehavior::kDataEof, false, false);
      const DecodeOutcome enabled =
          DecodeWithSimdutfFlag(bytes, FlushBehavior::kDataEof, false, true);
      EXPECT_FALSE(enabled.saw_error);
      EXPECT_FALSE(enabled.output.Is8Bit());
      EXPECT_EQ(enabled.output, control.output);
    }
  }

  // Case 3: >64 KB chunk exercising UTF-8 range boundaries (U+0100, U+07FF,
  // U+0800, U+D7FF, U+E000, BOM U+FEFF, literal U+FFFD, Unicode noncharacters
  // U+FDD0 / U+FFFE / U+FFFF, U+10000, and U+10FFFF) without setting saw_error.
  std::string boundary_utf16(68 * 1024, 'm');
  boundary_utf16.append(
      "\xC4\x80"          // U+0100
      "\xDF\xBF"          // U+07FF
      "\xE0\xA0\x80"      // U+0800
      "\xED\x9F\xBF"      // U+D7FF
      "\xEE\x80\x80"      // U+E000
      "\xEF\xB7\x90"      // U+FDD0
      "\xEF\xBB\xBF"      // U+FEFF (BOM)
      "\xEF\xBF\xBD"      // U+FFFD (valid literal replacement character)
      "\xEF\xBF\xBE"      // U+FFFE
      "\xEF\xBF\xBF"      // U+FFFF
      "\xF0\x90\x80\x80"  // U+10000
      "\xF4\x8F\xBF\xBF"  // U+10FFFF
  );
  {
    const auto bytes = base::as_bytes(base::span(boundary_utf16));
    const DecodeOutcome control =
        DecodeWithSimdutfFlag(bytes, FlushBehavior::kDataEof, false, false);
    const DecodeOutcome enabled =
        DecodeWithSimdutfFlag(bytes, FlushBehavior::kDataEof, false, true);
    EXPECT_FALSE(control.saw_error);
    EXPECT_FALSE(enabled.saw_error);
    EXPECT_FALSE(enabled.output.Is8Bit());
    EXPECT_EQ(enabled.output, control.output);
  }
}

TEST(SimdutfTextCodecUtf8Test, StreamingMultiChunkBoundarySplits) {
  // Multi-byte sequences to split across chunk boundaries:
  // - 2-byte Latin-1: U+00E9 (\xC3\xA9)
  // - 2-byte >0xFF:   U+0100 (\xC4\x80)
  // - 3-byte BMP:     U+2082 (\xE2\x82\x82) and U+2192 (\xE2\x86\x92)
  // - 4-byte astral:  U+1F600 (\xF0\x9F\x98\x80)
  const std::vector<std::string> sequences = {
      "\xC3\xA9",     "\xC4\x80",         "\xE2\x82\x82",
      "\xE2\x86\x92", "\xF0\x9F\x98\x80",
  };
  const std::vector<std::string> prefixes = {
      std::string(300, 'A'),
      std::string(300, 'B') + "\xC2\xA9",
      std::string(300, 'C') + "\xE2\x86\x92",
  };
  const std::vector<std::string> chunk2_bodies = {
      std::string(512, 'D'),
      std::string(512, 'D') + "\xC2\xA0\xC3\xBF",
      std::string(512, 'D') + "\xE2\x80\x94\xF0\x9F\x94\x8D",
  };

  for (const std::string& prefix : prefixes) {
    for (const std::string& body : chunk2_bodies) {
      for (const std::string& seq : sequences) {
        for (size_t split = 1; split < seq.size(); ++split) {
          std::vector<std::string> chunks;
          chunks.push_back(prefix + seq.substr(0, split));
          chunks.push_back(seq.substr(split) + body + seq.substr(0, split));
          chunks.push_back(seq.substr(split) + std::string(512, 'E'));

          auto run_stream = [&](bool enable_simdutf) {
            base::test::ScopedFeatureList scoped_feature_list;
            scoped_feature_list.InitWithFeatureState(
                features::kSimdutfTextCodecUtf8Decode, enable_simdutf);
            TextEncoding encoding("UTF-8");
            std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
            std::vector<String> outs;
            std::vector<bool> errs;
            StringBuilder combined;
            for (size_t i = 0; i < chunks.size(); ++i) {
              bool saw_error = false;
              const FlushBehavior flush = (i + 1 == chunks.size())
                                              ? FlushBehavior::kDataEof
                                              : FlushBehavior::kDoNotFlush;
              String part = codec->Decode(base::as_bytes(base::span(chunks[i])),
                                          flush, false, saw_error);
              outs.push_back(part);
              errs.push_back(saw_error);
              combined.Append(part);
            }
            return std::tuple{outs, errs, combined.ToString()};
          };

          const auto [outs_ctrl, errs_ctrl, full_ctrl] = run_stream(false);
          const auto [outs_simd, errs_simd, full_simd] = run_stream(true);
          EXPECT_EQ(errs_simd, errs_ctrl);
          EXPECT_EQ(outs_simd, outs_ctrl);
          for (size_t i = 0; i < outs_simd.size(); ++i) {
            EXPECT_EQ(outs_simd[i].Is8Bit(), outs_ctrl[i].Is8Bit());
            EXPECT_FALSE(errs_simd[i]);
          }
          EXPECT_EQ(full_simd, full_ctrl);
        }
      }
    }
  }

  // Also test incomplete trailing sequences in `c1` followed by >=256-byte
  // second chunks `c2` that either:
  // - fail immediately while completing `partial_sequence_`, or
  // - validly complete `partial_sequence_` (`leading_char` succeeds in
  //   TryDecodeWithSimdutf) and then hit malformed UTF-8 later in `c2` (via
  //   both `kNonAsciiSequenceLength[first_lead] == 0` and
  //   `!simdutf::validate_utf8(suffix)`),
  // verifying `partial_sequence_` is not mutated before validation succeeds,
  // under both `stop_on_error = false` and `stop_on_error = true`.
  const std::string c1 = std::string(300, 'P') + "\xE2";
  const std::vector<std::string> malformed_second_chunks = {
      "X" + std::string(512, 'R'),
      "\x82\x82" + std::string(300, 'R') + "\xED\xA0\x80" +
          std::string(64, 'R'),
      "\x82\x82" + std::string(300, 'R') + "\xFF" + std::string(64, 'R'),
      "\x82\x82" + std::string(300, 'R') + "\xC3\xA9\xED\xA0\x80" +
          std::string(64, 'R') + "\xE2",
  };
  const std::string c3 = "\x82\x82" + std::string(512, 'S');
  for (const std::string& c2 : malformed_second_chunks) {
    for (bool stop_on_error : {false, true}) {
      auto run_err_stream = [&](bool enable_simdutf) {
        base::test::ScopedFeatureList scoped_feature_list;
        scoped_feature_list.InitWithFeatureState(
            features::kSimdutfTextCodecUtf8Decode, enable_simdutf);
        TextEncoding encoding("UTF-8");
        std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
        bool e1 = false, e2 = false, e3 = false;
        String o1 =
            codec->Decode(base::as_bytes(base::span(c1)),
                          FlushBehavior::kDoNotFlush, stop_on_error, e1);
        String o2 =
            codec->Decode(base::as_bytes(base::span(c2)),
                          FlushBehavior::kDoNotFlush, stop_on_error, e2);
        String o3 = codec->Decode(base::as_bytes(base::span(c3)),
                                  FlushBehavior::kDataEof, stop_on_error, e3);
        return std::tuple{o1, e1, o2, e2, o3, e3};
      };
      EXPECT_EQ(run_err_stream(true), run_err_stream(false));
    }
  }

  // Also test incomplete trailing sequence flushed at EOF (kDataEof and
  // kFetchEof) both directly and via an empty final chunk.
  for (FlushBehavior eof_flush :
       {FlushBehavior::kDataEof, FlushBehavior::kFetchEof}) {
    for (std::string_view incomplete :
         {std::string_view("\xC3"), std::string_view("\xE2\x82"),
          std::string_view("\xF0\x9F\x98")}) {
      const std::string chunk1 =
          std::string(512, 'Q') + std::string(incomplete);
      ExpectSimdutfMatchesControl(chunk1, eof_flush, false);
      ExpectSimdutfMatchesControl(chunk1, eof_flush, true);

      auto run_truncated_stream = [&](bool enable_simdutf) {
        base::test::ScopedFeatureList scoped_feature_list;
        scoped_feature_list.InitWithFeatureState(
            features::kSimdutfTextCodecUtf8Decode, enable_simdutf);
        TextEncoding encoding("UTF-8");
        std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
        bool err1 = false;
        String out1 = codec->Decode(base::as_bytes(base::span(chunk1)),
                                    FlushBehavior::kDoNotFlush, false, err1);
        bool err2 = false;
        String out2 = codec->Decode({}, eof_flush, false, err2);
        return std::tuple{out1, err1, out2, err2};
      };

      const auto [o1_ctrl, e1_ctrl, o2_ctrl, e2_ctrl] =
          run_truncated_stream(false);
      const auto [o1_simd, e1_simd, o2_simd, e2_simd] =
          run_truncated_stream(true);
      EXPECT_FALSE(e1_simd);
      EXPECT_EQ(e1_simd, e1_ctrl);
      EXPECT_EQ(o1_simd, o1_ctrl);
      EXPECT_EQ(o1_simd.Is8Bit(), o1_ctrl.Is8Bit());
      EXPECT_TRUE(e2_simd);
      EXPECT_EQ(e2_simd, e2_ctrl);
      EXPECT_EQ(o2_simd, o2_ctrl);
    }
  }
}

TEST(SimdutfTextCodecUtf8Test, MalformedUtf8MatchesScalarDecoder) {
  const std::string pad(512, 'a');
  const std::vector<std::string> malformed_inputs = {
      // Stray continuation bytes.
      pad + "\x80" + pad,
      pad + "\xBF" + pad,
      // Overlong 2/3/4-byte sequences.
      pad + "\xC0\xAF" + pad,
      pad + "\xC1\xBF" + pad,
      pad + "\xE0\x80\xAF" + pad,
      pad + "\xF0\x80\x80\xAF" + pad,
      // UTF-16 surrogates encoded in UTF-8 (U+D800, U+DFFF).
      pad + "\xED\xA0\x80" + pad,
      pad + "\xED\xBF\xBF" + pad,
      // Out-of-range (> U+10FFFF) and invalid lead bytes.
      pad + "\xF4\x90\x80\x80" + pad,
      pad + "\xF5\x80\x80\x80" + pad,
      pad + "\xFF" + pad,
      // Truncated multi-byte sequences mid-chunk (maximal subpart semantics).
      pad + "\xC3" + "X" + pad,
      pad + "\xE2\x82" + "X" + pad,
      pad + "\xF0\x9F\x98" + "X" + pad,
      // Valid Latin-1 followed by malformed UTF-8 (rejected by upfront
      // simdutf::validate_utf8 before Latin-1 transcoding).
      pad + "\xC3\xA9" + pad + "\xED\xA0\x80" + pad,
      // Valid Latin-1 + valid 3-byte followed by malformed UTF-8 (rejected by
      // upfront simdutf::validate_utf8 before UTF-16 transcoding).
      pad + "\xC3\xA9\xE2\x86\x92" + pad + "\xF0\x80\x80\x80" + pad,
      // Invalid trailing 2-byte prefixes at chunk end (must NOT be saved as
      // valid partial sequences under kDoNotFlush).
      pad + "\xE0\x80",
      pad + "\xED\xA0",
      pad + "\xF0\x80",
      pad + "\xF4\x90",
      pad + "\xE2\xC3",
  };

  for (const std::string& input : malformed_inputs) {
    for (bool stop_on_error : {false, true}) {
      for (FlushBehavior flush :
           {FlushBehavior::kDataEof, FlushBehavior::kDoNotFlush}) {
        const auto bytes = base::as_bytes(base::span(input));
        const DecodeOutcome control =
            DecodeWithSimdutfFlag(bytes, flush, stop_on_error, false);
        const DecodeOutcome enabled =
            DecodeWithSimdutfFlag(bytes, flush, stop_on_error, true);
        EXPECT_TRUE(enabled.saw_error);
        EXPECT_EQ(enabled.saw_error, control.saw_error);
        EXPECT_EQ(enabled.output.Is8Bit(), control.output.Is8Bit());
        EXPECT_EQ(enabled.output, control.output);
      }
    }
  }

  // Verify pre-existing saw_error = true is preserved across valid >=256-byte
  // decodes (pure-ASCII, simdutf Latin-1, and simdutf UTF-16), matching the
  // scalar decoder.
  base::test::ScopedFeatureList scoped_feature_list(
      features::kSimdutfTextCodecUtf8Decode);
  TextEncoding encoding("UTF-8");
  for (const std::string& valid_input :
       {pad, pad + "\xC3\xA9", pad + "\xE2\x86\x92"}) {
    std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
    bool saw_error = true;
    String valid_out = codec->Decode(base::as_bytes(base::span(valid_input)),
                                     FlushBehavior::kDataEof, false, saw_error);
    EXPECT_TRUE(saw_error);
    EXPECT_FALSE(valid_out.empty());
  }
}

}  // namespace

}  // namespace blink
