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
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace blink {

TextEncoder* TextEncoder::Create(ExecutionContext* context,
                                 ExceptionState& exception_state) {
  WTF::TextEncoding encoding("UTF-8");
  return MakeGarbageCollected<TextEncoder>(encoding);
}

TextEncoder::TextEncoder(const WTF::TextEncoding& encoding)
    : encoding_(encoding), codec_(NewTextCodec(encoding)) {
  String name(encoding_.GetName());
  DCHECK_EQ(name, "UTF-8");
}

TextEncoder::~TextEncoder() = default;

String TextEncoder::encoding() const {
  String name = String(encoding_.GetName()).DeprecatedLower();
  DCHECK_EQ(name, "utf-8");
  return name;
}

NotShared<DOMUint8Array> TextEncoder::encode(const String& input) {
  std::string result;
  // Note that the UnencodableHandling here is never used since the
  // only possible encoding is UTF-8, which will use
  // U+FFFD-replacement rather than ASCII fallback substitution when
  // unencodable sequences (for instance, unpaired UTF-16 surrogates)
  // are present in the input.
  if (input.Is8Bit()) {
    result = codec_->Encode(input.Characters8(), input.length(),
                            WTF::kNoUnencodables);
  } else {
    result = codec_->Encode(input.Characters16(), input.length(),
                            WTF::kNoUnencodables);
  }

  const char* buffer = result.c_str();
  const unsigned char* unsigned_buffer =
      reinterpret_cast<const unsigned char*>(buffer);

  return NotShared<DOMUint8Array>(DOMUint8Array::Create(
      unsigned_buffer, static_cast<unsigned>(result.length())));
}

TextEncoderEncodeIntoResult* TextEncoder::encodeInto(
    const String& source,
    NotShared<DOMUint8Array>& destination) {
  TextEncoderEncodeIntoResult* encode_into_result =
      TextEncoderEncodeIntoResult::Create();

  TextCodec::EncodeIntoResult encode_into_result_data;
  unsigned char* destination_buffer = destination->Data();
  if (source.Is8Bit()) {
    encode_into_result_data =
        codec_->EncodeInto(source.Characters8(), source.length(),
                           destination_buffer, destination->length());
  } else {
    encode_into_result_data =
        codec_->EncodeInto(source.Characters16(), source.length(),
                           destination_buffer, destination->length());
  }

  encode_into_result->setRead(encode_into_result_data.code_units_read);
  encode_into_result->setWritten(encode_into_result_data.bytes_written);
  return encode_into_result;
}

}  // namespace blink
