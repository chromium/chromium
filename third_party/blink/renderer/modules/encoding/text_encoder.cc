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

#include "third_party/blink/renderer/modules/encoding/text_encoder.h"

#include <array>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_text_encoder_encode_into_result.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/encoding/encoding.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace blink {

// Controls if TextEncode will throw an exception when failed to allocate
// buffer.
BASE_FEATURE(kThrowExceptionWhenTextEncodeOOM,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls if TextEncode will use fast paths for short strings (direct
// ASCII copy and stack-buffer encoding) to avoid heap allocation overhead.
BASE_FEATURE(kTextEncoderFastPath, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

NotShared<DOMUint8Array> CreateDOMUint8Array(
    base::span<const uint8_t> bytes,
    ExceptionState& exception_state) {
  if (base::FeatureList::IsEnabled(kThrowExceptionWhenTextEncodeOOM)) {
    DOMUint8Array* result_array = DOMUint8Array::CreateOrNull(bytes);
    if (!result_array) {
      exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                        "Failed to allocate buffer.");
      return NotShared<DOMUint8Array>();
    }
    return NotShared<DOMUint8Array>(result_array);
  }
  return NotShared<DOMUint8Array>(DOMUint8Array::Create(bytes));
}

}  // namespace

TextEncoder* TextEncoder::Create(ExecutionContext* context,
                                 ExceptionState& exception_state) {
  return MakeGarbageCollected<TextEncoder>(Utf8Encoding());
}

TextEncoder::TextEncoder(const TextEncoding& encoding)
    : encoding_(encoding), codec_(NewTextCodec(encoding)) {
  DCHECK_EQ(encoding_.GetName(), "UTF-8");
  CHECK(codec_) << encoding_.GetName();
}

TextEncoder::~TextEncoder() = default;

String TextEncoder::encoding() const {
  String name = encoding_.GetName().GetString().ToAsciiLower();
  DCHECK_EQ(name, "utf-8");
  return name;
}

NotShared<DOMUint8Array> TextEncoder::encode(const String& input,
                                             ExceptionState& exception_state) {
  base::UmaHistogramCounts100000("Blink.TextEncoder.Encode.InputLength",
                                 input.length());

  if (base::FeatureList::IsEnabled(kTextEncoderFastPath)) {
    if (input.empty()) {
      base::UmaHistogramEnumeration("Blink.TextEncoder.Encode.FastPathResult",
                                    TextEncoderFastPathResult::kEmpty);
      return CreateDOMUint8Array({}, exception_state);
    }

    if (input.Is8Bit() && input.ContainsOnlyAsciiOrEmpty()) {
      base::UmaHistogramEnumeration("Blink.TextEncoder.Encode.FastPathResult",
                                    TextEncoderFastPathResult::kAscii8Bit);
      return CreateDOMUint8Array(input.Span8(), exception_state);
    }

    // Short non-ASCII strings can be encoded directly into a stack buffer to
    // avoid intermediate std::string heap allocation.
    // The maximum UTF-8 expansion per UTF-16 code unit is 3 bytes (surrogates
    // take 2 code units and at most 4 bytes).
    constexpr size_t kStackBufferSize = 256;
    if (input.length() <= kStackBufferSize / 3) {
      base::UmaHistogramEnumeration("Blink.TextEncoder.Encode.FastPathResult",
                                    TextEncoderFastPathResult::kShortNonAscii);
      std::array<uint8_t, kStackBufferSize> stack_buffer;
      TextCodec::EncodeIntoResult encode_into_result =
          VisitCharacters(input, [this, &stack_buffer](auto chars) {
            return codec_->EncodeInto(chars, base::span(stack_buffer));
          });
      CHECK_EQ(encode_into_result.code_units_read, input.length());
      return CreateDOMUint8Array(
          base::span(stack_buffer).first(encode_into_result.bytes_written),
          exception_state);
    }

    base::UmaHistogramEnumeration("Blink.TextEncoder.Encode.FastPathResult",
                                  TextEncoderFastPathResult::kLongString);
  } else {
    base::UmaHistogramEnumeration("Blink.TextEncoder.Encode.FastPathResult",
                                  TextEncoderFastPathResult::kDisabled);
  }

  // Note that the UnencodableHandling here is never used since the
  // only possible encoding is UTF-8, which will use
  // U+FFFD-replacement rather than ASCII fallback substitution when
  // unencodable sequences (for instance, unpaired UTF-16 surrogates)
  // are present in the input.
  String non_null_input = input.IsNull() ? g_empty_string : input;
  std::string result = VisitCharacters(non_null_input, [this](auto chars) {
    return codec_->Encode(chars, UnencodableHandling::kNone);
  });
  return CreateDOMUint8Array(base::as_byte_span(result), exception_state);
}

TextEncoderEncodeIntoResult* TextEncoder::encodeInto(
    const String& source,
    NotShared<DOMUint8Array>& destination) {
  TextEncoderEncodeIntoResult* encode_into_result =
      TextEncoderEncodeIntoResult::Create();

  TextCodec::EncodeIntoResult encode_into_result_data =
      VisitCharacters(source, [this, &destination](auto chars) {
        return codec_->EncodeInto(chars, destination->ByteSpan());
      });
  encode_into_result->setRead(encode_into_result_data.code_units_read);
  encode_into_result->setWritten(encode_into_result_data.bytes_written);
  return encode_into_result;
}

}  // namespace blink
