// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encoding/text_decoder.h"

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
  DummyExceptionStateForTesting exception_state;
  TextDecoder* decoder = TextDecoder::Create(
      "utf-8", TextDecoderOptions::Create(), exception_state);
  ASSERT_FALSE(exception_state.HadException());

  DOMArrayBuffer* buffer = DOMArrayBuffer::Create(kStringMaxUCharLength, 1);
  decoder->decode(buffer->ByteSpan(), TextDecodeOptions::Create(),
                  exception_state);
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

}  // namespace
}  // namespace blink
