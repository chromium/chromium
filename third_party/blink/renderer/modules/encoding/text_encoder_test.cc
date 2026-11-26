// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encoding/text_encoder.h"

#include <array>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {
namespace {

std::vector<uint8_t> CopyToVector(v8::Local<v8::Uint8Array> array) {
  std::vector<uint8_t> result(array->ByteLength());
  if (!result.empty()) {
    array->CopyContents(result.data(), result.size());
  }
  return result;
}

class TextEncoderTest : public testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;
    if (std::get<0>(GetParam())) {
      enabled.push_back(kTextEncoderFastPath);
    } else {
      disabled.push_back(kTextEncoderFastPath);
    }
    if (std::get<1>(GetParam())) {
      enabled.push_back(kTextEncoderDirectEncode);
    } else {
      disabled.push_back(kTextEncoderDirectEncode);
    }
    feature_list_.InitWithFeatures(enabled, disabled);
  }

 private:
  test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TextEncoderTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(TextEncoderTest, EncodeEmptyAndNullString) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  // Empty string
  auto result_empty =
      encoder->encode(scope.GetScriptState(), "", exception_state)
          .ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(0u, result_empty->ByteLength());

  // Null string (from C++)
  auto result_null =
      encoder->encode(scope.GetScriptState(), String(), exception_state)
          .ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(0u, result_null->ByteLength());
}

TEST_P(TextEncoderTest, EncodeAscii) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  String ascii_string("Hello, World!");
  auto result =
      encoder->encode(scope.GetScriptState(), ascii_string, exception_state)
          .ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsEmpty());
  EXPECT_EQ(13u, result->ByteLength());

  auto bytes = CopyToVector(result);
  EXPECT_EQ("Hello, World!",
            std::string_view(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size()));
}

TEST_P(TextEncoderTest, EncodeShortNonAscii) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  // Non-ASCII string with <= 85 code units (exercises stack buffer)
  String utf8_string(u"Hello \U0001F680 \u2713");
  auto result =
      encoder->encode(scope.GetScriptState(), utf8_string, exception_state)
          .ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsEmpty());

  auto bytes = CopyToVector(result);
  // "Hello " (6) + Rocket 🚀 (4) + " " (1) + Checkmark ✓ (3) = 14 bytes
  EXPECT_EQ(14u, bytes.size());
  const uint8_t expected[] = {
      'H',  'e',  'l',  'l',  'o',  ' ',
      0xF0, 0x9F, 0x9A, 0x80,  // Rocket 🚀
      ' ',
      0xE2, 0x9C, 0x93  // Checkmark ✓
  };
  EXPECT_THAT(bytes, ::testing::ElementsAreArray(expected));
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

  auto result =
      encoder->encode(scope.GetScriptState(), latin1_string, exception_state)
          .ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsEmpty());

  auto bytes = CopyToVector(result);
  // 'c', 'a', 'f' (3 bytes) + 'é' in UTF-8: 0xC3, 0xA9 (2 bytes) = 5 bytes
  EXPECT_EQ(5u, bytes.size());
  const uint8_t expected[] = {'c', 'a', 'f', 0xC3, 0xA9};
  EXPECT_THAT(bytes, ::testing::ElementsAreArray(expected));
}

TEST_P(TextEncoderTest, EncodeUnpairedSurrogates) {
  V8TestingScope scope;
  TextEncoder* encoder =
      MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;

  // Unpaired lead surrogate (0xD800) and trail surrogate (0xDFFF)
  const UChar surrogates[] = {0xD800, 'a', 0xDFFF};
  String surrogate_string = String(base::span(surrogates));

  auto result =
      encoder->encode(scope.GetScriptState(), surrogate_string, exception_state)
          .ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsEmpty());

  auto bytes = CopyToVector(result);
  // U+FFFD (3 bytes) + 'a' (1 byte) + U+FFFD (3 bytes) = 7 bytes
  EXPECT_EQ(7u, bytes.size());
  const uint8_t expected[] = {
      0xEF, 0xBF, 0xBD,  // U+FFFD
      'a',
      0xEF, 0xBF, 0xBD   // U+FFFD
  };
  EXPECT_THAT(bytes, ::testing::ElementsAreArray(expected));
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
  String str_85 = builder_85.ReleaseString();
  EXPECT_EQ(85u, str_85.length());

  auto result_85 =
      encoder->encode(scope.GetScriptState(), str_85, exception_state)
          .ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result_85.IsEmpty());
  EXPECT_EQ(255u, result_85->ByteLength());
  auto bytes_85 = CopyToVector(result_85);
  for (size_t i = 0; i < 85; ++i) {
    EXPECT_EQ(bytes_85[i * 3 + 0], 0xE3);
    EXPECT_EQ(bytes_85[i * 3 + 1], 0x81);
    EXPECT_EQ(bytes_85[i * 3 + 2], 0x82);
  }

  // One beyond boundary: 86 3-byte code points.
  // Exceeds 85 code units, safely falls back to heap path.
  StringBuilder builder_86;
  for (int i = 0; i < 86; ++i) {
    builder_86.Append(StringView(u"あ"));
  }
  String str_86 = builder_86.ReleaseString();
  EXPECT_EQ(86u, str_86.length());

  auto result_86 =
      encoder->encode(scope.GetScriptState(), str_86, exception_state)
          .ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result_86.IsEmpty());
  EXPECT_EQ(258u, result_86->ByteLength());
  auto bytes_86 = CopyToVector(result_86);
  for (size_t i = 0; i < 86; ++i) {
    EXPECT_EQ(bytes_86[i * 3 + 0], 0xE3);
    EXPECT_EQ(bytes_86[i * 3 + 1], 0x81);
    EXPECT_EQ(bytes_86[i * 3 + 2], 0x82);
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
  String long_string = builder.ReleaseString();
  EXPECT_GT(long_string.length(), 85u);

  auto result =
      encoder->encode(scope.GetScriptState(), long_string, exception_state)
          .ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsEmpty());
  // 100 rockets * 4 bytes = 400 bytes
  EXPECT_EQ(400u, result->ByteLength());
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

  auto result = encoder->encode(scope.GetScriptState(), str, exception_state)
                    .ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsEmpty());
  // Each U+00E9 encodes to 2 bytes in UTF-8: 0xC3, 0xA9.
  EXPECT_EQ(200u, result->ByteLength());
  auto bytes = CopyToVector(result);
  for (size_t i = 0; i < 100; ++i) {
    EXPECT_EQ(bytes[i * 2 + 0], 0xC3);
    EXPECT_EQ(bytes[i * 2 + 1], 0xA9);
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

  auto result = encoder->encode(scope.GetScriptState(), str, exception_state)
                    .ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_FALSE(result.IsEmpty());
  EXPECT_EQ(11u, result->ByteLength());
  const uint8_t expected[] = {'h', 'e', 'l', 'l', 'o', ' ',
                              'w', 'o', 'r', 'l', 'd'};
  EXPECT_THAT(CopyToVector(result), ::testing::ElementsAreArray(expected));
}

TEST_P(TextEncoderTest, FastPathAndLengthHistograms) {
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;
  V8TestingScope scope;
  TextEncoder* encoder = MakeGarbageCollected<TextEncoder>(Utf8Encoding());
  DummyExceptionStateForTesting exception_state;
  const bool fast_path_enabled = std::get<0>(GetParam());

  {
    base::HistogramTester histogram_tester;
    encoder->encode(scope.GetScriptState(), "", exception_state);
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
    encoder->encode(scope.GetScriptState(), "hello", exception_state);
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
    String short_utf8 = builder.ReleaseString();
    encoder->encode(scope.GetScriptState(), short_utf8, exception_state);
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
    String long_str = builder.ReleaseString();
    encoder->encode(scope.GetScriptState(), long_str, exception_state);
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
