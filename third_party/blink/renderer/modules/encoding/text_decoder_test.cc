// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encoding/text_decoder.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_text_decode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_text_decoder_options.h"
#include "third_party/blink/renderer/core/streams/text_decoder_transformer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {
namespace {

#if defined(ARCH_CPU_64_BITS)
TEST(TextDecoderTest, DecodeOversizedSpanThrowsRangeError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  TextDecoder* decoder = TextDecoder::Create(
      "utf-8", TextDecoderOptions::Create(), exception_state);
  ASSERT_FALSE(exception_state.HadException());

  DOMArrayBuffer* buffer = DOMArrayBuffer::Create(kStringMaxUCharLength, 1);
  decoder->decode(scope.GetIsolate(), buffer->ByteSpan(),
                  TextDecodeOptions::Create(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kRangeError, exception_state.CodeAs<ESErrorType>());
}

TEST(TextDecoderTest, TransformerOversizedBufferThrowsRangeError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto* transformer = MakeGarbageCollected<TextDecoderTransformer>(
      scope.GetScriptState(), TextEncoding("utf-8"), /*fatal=*/false,
      /*ignore_bom=*/false);

  // Allocate an ArrayBuffer larger than `kStringMaxUCharLength - 3` (approx 1
  // GiB) but within `partition_alloc::MaxAllocationSize()` so that
  // V8BufferSource::Create succeeds and reaches the size check in
  // TextDecoderTransformer::Transform.
  DOMArrayBuffer* buffer = DOMArrayBuffer::Create(kStringMaxUCharLength, 1);
  v8::Local<v8::Value> v8_buffer =
      ToV8Traits<DOMArrayBuffer>::ToV8(scope.GetScriptState(), buffer);

  DummyExceptionStateForTesting exception_state;
  transformer->Transform(v8_buffer, /*controller=*/nullptr, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kRangeError, exception_state.CodeAs<ESErrorType>());
}
#endif

class TextDecoderFastPathTest : public testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(kTextDecoderFastPath);
    } else {
      feature_list_.InitAndDisableFeature(kTextDecoderFastPath);
    }
  }

  String Utf8(std::string_view s) {
    return String::FromUtf8(base::as_byte_span(s));
  }

  String DecodeToCoreString(V8TestingScope& scope,
                           TextDecoder* decoder,
                           base::span<const uint8_t> bytes,
                           bool stream,
                           ExceptionState& exception_state) {
    v8::MaybeLocal<v8::String> maybe_v8_str;
    if (stream) {
      auto* options = TextDecodeOptions::Create();
      options->setStream(true);
      maybe_v8_str = decoder->decode(scope.GetIsolate(), bytes, options,
                                     exception_state);
    } else {
      maybe_v8_str =
          decoder->decode(scope.GetIsolate(), bytes, exception_state);
    }
    v8::Local<v8::String> v8_str;
    if (!maybe_v8_str.ToLocal(&v8_str)) {
      return String();
    }
    return ToCoreString(scope.GetIsolate(), v8_str);
  }

  test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(TextDecoderFastPathTest, EmptyAndAscii) {
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  TextDecoder* decoder = TextDecoder::Create(
      "utf-8", TextDecoderOptions::Create(), exception_state);
  ASSERT_FALSE(exception_state.HadException());

  v8::Local<v8::String> no_arg =
      decoder->decode(scope.GetIsolate(), exception_state).ToLocalChecked();
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(0, no_arg->Length());

  EXPECT_EQ("", DecodeToCoreString(scope, decoder, {}, /*stream=*/false,
                                   exception_state));
  EXPECT_FALSE(exception_state.HadException());

  const std::string ascii = "hello world";
  EXPECT_EQ("hello world",
            DecodeToCoreString(scope, decoder, base::as_byte_span(ascii),
                               /*stream=*/false, exception_state));
  EXPECT_FALSE(exception_state.HadException());

  // Check 128-byte fast-path boundary and 129-byte fallback.
  std::string len_128(128, 'a');
  EXPECT_EQ(Utf8(len_128),
            DecodeToCoreString(scope, decoder, base::as_byte_span(len_128),
                               /*stream=*/false, exception_state));
  std::string len_129(129, 'b');
  EXPECT_EQ(Utf8(len_129),
            DecodeToCoreString(scope, decoder, base::as_byte_span(len_129),
                               /*stream=*/false, exception_state));
}

TEST_P(TextDecoderFastPathTest, ShortUtf8Latin1AndBmpAndSurrogates) {
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  TextDecoder* decoder = TextDecoder::Create(
      "utf-8", TextDecoderOptions::Create(), exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // 8-bit Latin-1 non-ASCII: "café"
  const std::string latin1 = "caf\xC3\xA9";
  EXPECT_EQ(Utf8(latin1),
            DecodeToCoreString(scope, decoder, base::as_byte_span(latin1),
                               /*stream=*/false, exception_state));
  EXPECT_FALSE(exception_state.HadException());

  // 16-bit BMP + surrogate pair: "hello 世界 😀"
  const std::string utf8 = "hello \xE4\xB8\x96\xE7\x95\x8C \xF0\x9F\x98\x80";
  EXPECT_EQ(Utf8(utf8),
            DecodeToCoreString(scope, decoder, base::as_byte_span(utf8),
                               /*stream=*/false, exception_state));
  EXPECT_FALSE(exception_state.HadException());

  // 128-byte non-ASCII boundary (64 x 2-byte UTF-8) vs 130-byte fallback.
  std::string non_ascii_128;
  for (int i = 0; i < 64; ++i) {
    non_ascii_128 += "\xC3\xA9";
  }
  EXPECT_EQ(Utf8(non_ascii_128),
            DecodeToCoreString(scope, decoder, base::as_byte_span(non_ascii_128),
                               /*stream=*/false, exception_state));
  std::string non_ascii_130 = non_ascii_128 + "\xC3\xA9";
  EXPECT_EQ(Utf8(non_ascii_130),
            DecodeToCoreString(scope, decoder, base::as_byte_span(non_ascii_130),
                               /*stream=*/false, exception_state));
}

TEST_P(TextDecoderFastPathTest, BomHandling) {
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  auto* strip_opts = TextDecoderOptions::Create();
  strip_opts->setIgnoreBOM(false);
  TextDecoder* strip_decoder =
      TextDecoder::Create("utf-8", strip_opts, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  const std::string bom_only = "\xEF\xBB\xBF";
  EXPECT_EQ("",
            DecodeToCoreString(scope, strip_decoder, base::as_byte_span(bom_only),
                               /*stream=*/false, exception_state));

  const std::string bom_text = "\xEF\xBB\xBFhello\xC3\xA9";
  EXPECT_EQ(Utf8("hello\xC3\xA9"),
            DecodeToCoreString(scope, strip_decoder, base::as_byte_span(bom_text),
                               /*stream=*/false, exception_state));

  auto* keep_opts = TextDecoderOptions::Create();
  keep_opts->setIgnoreBOM(true);
  TextDecoder* keep_decoder =
      TextDecoder::Create("utf-8", keep_opts, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_EQ(Utf8(bom_text),
            DecodeToCoreString(scope, keep_decoder, base::as_byte_span(bom_text),
                               /*stream=*/false, exception_state));
}

TEST_P(TextDecoderFastPathTest, InvalidUtf8FatalAndReplacement) {
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  TextDecoder* lenient = TextDecoder::Create(
      "utf-8", TextDecoderOptions::Create(), exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // Invalid byte and incomplete trailing 3-byte sequence.
  const std::array<uint8_t, 3> invalid_bytes = {'a', 0xE4, 0xB8};
  EXPECT_EQ(String(u"a\uFFFD"),
            DecodeToCoreString(scope, lenient, invalid_bytes, /*stream=*/false,
                               exception_state));
  EXPECT_FALSE(exception_state.HadException());

  auto* fatal_opts = TextDecoderOptions::Create();
  fatal_opts->setFatal(true);
  TextDecoder* fatal = TextDecoder::Create("utf-8", fatal_opts, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  DecodeToCoreString(scope, fatal, invalid_bytes, /*stream=*/false,
                    exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
}

TEST_P(TextDecoderFastPathTest, StreamingAndNonUtf8Fallback) {
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  TextDecoder* decoder = TextDecoder::Create(
      "utf-8", TextDecoderOptions::Create(), exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // Split 3-byte character "世" (0xE4 0xB8 0x96) across streaming calls.
  const std::array<uint8_t, 2> part1 = {0xE4, 0xB8};
  const std::array<uint8_t, 1> part2 = {0x96};
  EXPECT_EQ("", DecodeToCoreString(scope, decoder, part1, /*stream=*/true,
                                   exception_state));
  EXPECT_EQ(Utf8("\xE4\xB8\x96"),
            DecodeToCoreString(scope, decoder, part2, /*stream=*/false,
                               exception_state));

  TextDecoder* latin1_decoder = TextDecoder::Create(
      "windows-1252", TextDecoderOptions::Create(), exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const std::array<uint8_t, 2> win1252 = {0x80, 'a'};
  EXPECT_EQ(String(u"\u20ACa"),
            DecodeToCoreString(scope, latin1_decoder, win1252, /*stream=*/false,
                               exception_state));
}

INSTANTIATE_TEST_SUITE_P(All, TextDecoderFastPathTest, testing::Bool());

}  // namespace
}  // namespace blink
