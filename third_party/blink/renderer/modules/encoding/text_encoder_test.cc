// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encoding/text_encoder.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {
namespace {

class TextEncoderTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(kTextEncoderFastPath);
    } else {
      feature_list_.InitAndDisableFeature(kTextEncoderFastPath);
    }
  }

 private:
  test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, TextEncoderTest, testing::Bool());

TEST_P(TextEncoderTest, EncodeEmptyAndNullString) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  // Empty string
  auto result_empty = encoder->encode("", exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result_empty.IsNull());
  EXPECT_EQ(0u, result_empty->byteLength());

  // Null string (from C++)
  auto result_null = encoder->encode(String(), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result_null.IsNull());
  EXPECT_EQ(0u, result_null->byteLength());
}

TEST_P(TextEncoderTest, EncodeAscii) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  String ascii_string("Hello, World!");
  auto result = encoder->encode(ascii_string, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsNull());
  EXPECT_EQ(13u, result->byteLength());

  auto span = base::as_bytes(result->ByteSpan());
  EXPECT_EQ("Hello, World!",
            std::string_view(reinterpret_cast<const char*>(span.data()),
                             span.size()));
}

TEST_P(TextEncoderTest, EncodeShortNonAscii) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  // Non-ASCII string with <= 85 code units (exercises stack buffer)
  String utf8_string(u"Hello \U0001F680 \u2713");
  auto result = encoder->encode(utf8_string, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsNull());

  auto span = base::as_bytes(result->ByteSpan());
  // "Hello " (6) + Rocket 🚀 (4) + " " (1) + Checkmark ✓ (3) = 14 bytes
  EXPECT_EQ(14u, span.size());
  const uint8_t expected[] = {
      'H',  'e',  'l',  'l',  'o',  ' ',
      0xF0, 0x9F, 0x9A, 0x80,  // Rocket 🚀
      ' ',
      0xE2, 0x9C, 0x93  // Checkmark ✓
  };
  EXPECT_EQ(base::span(expected), span);
}

TEST_P(TextEncoderTest, Encode8BitLatin1NonAscii) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  // 8-bit string containing non-ASCII Latin-1 character (0xE9 is 'é').
  // This is an 8-bit string (input.Is8Bit() == true), but not ASCII,
  // so it exercises the stack buffer path with LChar characters.
  const LChar latin1_chars[] = {'c', 'a', 'f', 0xE9};
  String latin1_string = String(base::span(latin1_chars));
  ASSERT_TRUE(latin1_string.Is8Bit());
  ASSERT_FALSE(latin1_string.ContainsOnlyAsciiOrEmpty());

  auto result = encoder->encode(latin1_string, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsNull());

  auto span = base::as_bytes(result->ByteSpan());
  // 'c', 'a', 'f' (3 bytes) + 'é' in UTF-8: 0xC3, 0xA9 (2 bytes) = 5 bytes
  EXPECT_EQ(5u, span.size());
  const uint8_t expected[] = {'c', 'a', 'f', 0xC3, 0xA9};
  EXPECT_EQ(base::span(expected), span);
}

TEST_P(TextEncoderTest, EncodeUnpairedSurrogates) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  // Unpaired lead surrogate (0xD800) and trail surrogate (0xDFFF)
  const UChar surrogates[] = {0xD800, 'a', 0xDFFF};
  String surrogate_string = String(base::span(surrogates));

  auto result = encoder->encode(surrogate_string, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsNull());

  auto span = base::as_bytes(result->ByteSpan());
  // U+FFFD (3 bytes) + 'a' (1 byte) + U+FFFD (3 bytes) = 7 bytes
  EXPECT_EQ(7u, span.size());
  const uint8_t expected[] = {
      0xEF, 0xBF, 0xBD,  // U+FFFD
      'a',
      0xEF, 0xBF, 0xBD   // U+FFFD
  };
  EXPECT_EQ(base::span(expected), span);
}

TEST_P(TextEncoderTest, EncodeStackBufferBoundary85And86) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  // Exact boundary: 85 3-byte code points (e.g. Japanese 'あ' U+3042).
  // 85 code units * 3 bytes = 255 bytes, fits in 256-byte stack buffer.
  StringBuilder builder_85;
  for (int i = 0; i < 85; ++i) {
    builder_85.Append(StringView(u"あ"));
  }
  String str_85 = builder_85.ToString();
  EXPECT_EQ(85u, str_85.length());

  auto result_85 = encoder->encode(str_85, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result_85.IsNull());
  EXPECT_EQ(255u, result_85->byteLength());
  auto span_85 = base::as_bytes(result_85->ByteSpan());
  for (size_t i = 0; i < 85; ++i) {
    EXPECT_EQ(span_85[i * 3 + 0], 0xE3);
    EXPECT_EQ(span_85[i * 3 + 1], 0x81);
    EXPECT_EQ(span_85[i * 3 + 2], 0x82);
  }

  // One beyond boundary: 86 3-byte code points.
  // Exceeds 85 code units, safely falls back to heap path.
  StringBuilder builder_86;
  for (int i = 0; i < 86; ++i) {
    builder_86.Append(StringView(u"あ"));
  }
  String str_86 = builder_86.ToString();
  EXPECT_EQ(86u, str_86.length());

  auto result_86 = encoder->encode(str_86, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result_86.IsNull());
  EXPECT_EQ(258u, result_86->byteLength());
  auto span_86 = base::as_bytes(result_86->ByteSpan());
  for (size_t i = 0; i < 86; ++i) {
    EXPECT_EQ(span_86[i * 3 + 0], 0xE3);
    EXPECT_EQ(span_86[i * 3 + 1], 0x81);
    EXPECT_EQ(span_86[i * 3 + 2], 0x82);
  }
}

TEST_P(TextEncoderTest, EncodeLongNonAsciiFallback) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  // String longer than 85 code units (exercises heap fallback)
  StringBuilder builder;
  for (int i = 0; i < 100; ++i) {
    builder.Append(StringView(u"🚀"));
  }
  String long_string = builder.ToString();
  EXPECT_GT(long_string.length(), 85u);

  auto result = encoder->encode(long_string, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsNull());
  // 100 rockets * 4 bytes = 400 bytes
  EXPECT_EQ(400u, result->byteLength());
}

TEST_P(TextEncoderTest, Encode16BitLatin1LongString) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  // 16-bit string containing Latin-1 non-ASCII characters (> 0x7F, e.g. U+00E9 'é').
  // Length 100 > 85, so it exceeds the 256-byte stack buffer and falls back to heap.
  Vector<UChar> chars(100, 0x00E9);
  String str = String(base::span(chars));
  EXPECT_FALSE(str.Is8Bit());
  EXPECT_FALSE(str.ContainsOnlyAsciiOrEmpty());
  EXPECT_EQ(100u, str.length());

  auto result = encoder->encode(str, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsNull());
  // Each U+00E9 encodes to 2 bytes in UTF-8: 0xC3, 0xA9.
  EXPECT_EQ(200u, result->byteLength());
  auto span = base::as_bytes(result->ByteSpan());
  for (size_t i = 0; i < 100; ++i) {
    EXPECT_EQ(span[i * 2 + 0], 0xC3);
    EXPECT_EQ(span[i * 2 + 1], 0xA9);
  }
}

TEST_P(TextEncoderTest, Encode16BitAsciiString) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  // 16-bit string containing only ASCII characters.
  // Is8Bit() is false, but ContainsOnlyAsciiOrEmpty() is true.
  const UChar chars[] = {'h', 'e', 'l', 'l', 'o', ' ',
                         'w', 'o', 'r', 'l', 'd'};
  String str = String(base::span(chars));
  EXPECT_FALSE(str.Is8Bit());
  EXPECT_TRUE(str.ContainsOnlyAsciiOrEmpty());
  EXPECT_EQ(11u, str.length());

  auto result = encoder->encode(str, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsNull());
  EXPECT_EQ(11u, result->byteLength());
  const uint8_t expected[] = {'h', 'e', 'l', 'l', 'o', ' ',
                              'w', 'o', 'r', 'l', 'd'};
  EXPECT_EQ(base::span(expected), base::as_bytes(result->ByteSpan()));
}

TEST_P(TextEncoderTest, FastPathAndLengthHistograms) {
  V8TestingScope scope;
  TextEncoder* encoder = MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;
  const bool fast_path_enabled = GetParam();

  {
    base::HistogramTester histogram_tester;
    encoder->encode("", exception_state);
    histogram_tester.ExpectUniqueSample("Blink.TextEncoder.Encode.InputLength",
                                        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Blink.TextEncoder.Encode.FastPathResult",
        fast_path_enabled ? TextEncoderFastPathResult::kEmpty
                          : TextEncoderFastPathResult::kDisabled,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    encoder->encode("hello", exception_state);
    histogram_tester.ExpectUniqueSample("Blink.TextEncoder.Encode.InputLength",
                                        5, 1);
    histogram_tester.ExpectUniqueSample(
        "Blink.TextEncoder.Encode.FastPathResult",
        fast_path_enabled ? TextEncoderFastPathResult::kAscii8Bit
                          : TextEncoderFastPathResult::kDisabled,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    // Short non-ASCII: 3 emojis = 6 UTF-16 code units (<= 85)
    StringBuilder builder;
    builder.Append(StringView(u"🚀🎉✨"));
    String short_utf8 = builder.ToString();
    encoder->encode(short_utf8, exception_state);
    histogram_tester.ExpectUniqueSample("Blink.TextEncoder.Encode.InputLength",
                                        short_utf8.length(), 1);
    histogram_tester.ExpectUniqueSample(
        "Blink.TextEncoder.Encode.FastPathResult",
        fast_path_enabled ? TextEncoderFastPathResult::kShortNonAscii
                          : TextEncoderFastPathResult::kDisabled,
        1);
  }

  {
    base::HistogramTester histogram_tester;
    // Long string: 100 code units (> 85)
    StringBuilder builder;
    for (int i = 0; i < 100; ++i) {
      builder.Append(static_cast<UChar>(0x00E9));
    }
    String long_str = builder.ToString();
    encoder->encode(long_str, exception_state);
    histogram_tester.ExpectUniqueSample("Blink.TextEncoder.Encode.InputLength",
                                        100, 1);
    histogram_tester.ExpectUniqueSample(
        "Blink.TextEncoder.Encode.FastPathResult",
        fast_path_enabled ? TextEncoderFastPathResult::kLongString
                          : TextEncoderFastPathResult::kDisabled,
        1);
  }
}

}  // namespace
}  // namespace blink
