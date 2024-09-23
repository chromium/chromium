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
             "ThrowExceptionWhenTextEncodeOOM",
             base::FEATURE_ENABLED_BY_DEFAULT);

TextEncoder* TextEncoder::Create(ExecutionContext* context,
                                 ExceptionState& exception_state) {
  return MakeGarbageCollected<TextEncoder>(UTF8Encoding());
}

TextEncoder::TextEncoder(const WTF::TextEncoding& encoding)
    : encoding_(encoding), codec_(NewTextCodec(encoding)) {
  DCHECK_EQ(encoding_.GetName(), "UTF-8");
}

TextEncoder::~TextEncoder() = default;

String TextEncoder::encoding() const {
  String name = encoding_.GetName().GetString().DeprecatedLower();
  DCHECK_EQ(name, "utf-8");
  return name;
}

NotShared<DOMUint8Array> TextEncoder::encode(const String& input,
                                             ExceptionState& exception_state) {
  // Note that the UnencodableHandling here is never used since the
  // only possible encoding is UTF-8, which will use
  // U+FFFD-replacement rather than ASCII fallback substitution when
  // unencodable sequences (for instance, unpaired UTF-16 surrogates)
  // are present in the input.
  std::string result = WTF::VisitCharacters(input, [this](auto chars) {
    return codec_->Encode(chars, WTF::kNoUnencodables);
  });
  if (base::FeatureList::IsEnabled(kThrowExceptionWhenTextEncodeOOM)) {
    NotShared<DOMUint8Array> result_array(
        DOMUint8Array::CreateOrNull(base::as_byte_span(result)));
    if (result_array.IsNull()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                        "Failed to allocate buffer.");
    }
    return result_array;
  }
  return NotShared<DOMUint8Array>(
      DOMUint8Array::Create(base::as_byte_span(result)));
}

TextEncoderEncodeIntoResult* TextEncoder::encodeInto(
    const String& source,
    NotShared<DOMUint8Array>& destination) {
  TextEncoderEncodeIntoResult* encode_into_result =
      TextEncoderEncodeIntoResult::Create();

  TextCodec::EncodeIntoResult encode_into_result_data =
      WTF::VisitCharacters(source, [this, &destination](auto chars) {
        return codec_->EncodeInto(chars, destination->ByteSpan());
      });
  encode_into_result->setRead(encode_into_result_data.code_units_read);
  encode_into_result->setWritten(encode_into_result_data.bytes_written);
  return encode_into_result;
}

}  // namespace blink
