/*
 * Copyright (C) 2007, 2008 Apple, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/text/text_codec_user_defined.h"

#include <memory>
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

void TextCodecUserDefined::RegisterEncodingNames(
    EncodingNameRegistrar registrar) {
  registrar("x-user-defined", "x-user-defined");
}

static std::unique_ptr<TextCodec> NewStreamingTextDecoderUserDefined(
    const TextEncoding&,
    const void*) {
  return std::make_unique<TextCodecUserDefined>();
}

void TextCodecUserDefined::RegisterCodecs(TextCodecRegistrar registrar) {
  registrar("x-user-defined", NewStreamingTextDecoderUserDefined, nullptr);
}

String TextCodecUserDefined::Decode(const char* bytes,
                                    wtf_size_t length,
                                    FlushBehavior,
                                    bool,
                                    bool&) {
  StringBuilder result;
  result.ReserveCapacity(length);

  for (wtf_size_t i = 0; i < length; ++i) {
    signed char c = bytes[i];
    result.Append(static_cast<UChar>(c & 0xF7FF));
  }

  return result.ToString();
}

template <typename CharType>
static std::string EncodeComplexUserDefined(const CharType* characters,
                                            wtf_size_t length,
                                            UnencodableHandling handling) {
  DCHECK_NE(handling, kNoUnencodables);
  wtf_size_t target_length = length;
  Vector<char> result(target_length);
  char* bytes = result.data();

  wtf_size_t result_length = 0;
  for (wtf_size_t i = 0; i < length;) {
    UChar32 c;
    // TODO(jsbell): Will the input for x-user-defined ever be LChars?
    U16_NEXT(characters, i, length, c);
    // If the input was a surrogate pair (non-BMP character) then we
    // overestimated the length.
    if (c > 0xffff)
      --target_length;
    signed char signed_byte = static_cast<signed char>(c);
    if ((signed_byte & 0xF7FF) == c) {
      bytes[result_length++] = signed_byte;
    } else {
      // No way to encode this character with x-user-defined.
      UnencodableReplacementArray replacement;
      int replacement_length =
          TextCodec::GetUnencodableReplacement(c, handling, replacement);
      DCHECK_GT(replacement_length, 0);
      // Only one char was initially reserved per input character, so grow if
      // necessary.
      target_length += replacement_length - 1;
      if (target_length > result.size()) {
        result.Grow(target_length);
        bytes = result.data();
      }
      memcpy(bytes + result_length, replacement, replacement_length);
      result_length += replacement_length;
    }
  }

  return std::string(bytes, result_length);
}

template <typename CharType>
std::string TextCodecUserDefined::EncodeCommon(const CharType* characters,
                                               wtf_size_t length,
                                               UnencodableHandling handling) {
  std::string result(length, '\0');

  // Convert the string a fast way and simultaneously do an efficient check to
  // see if it's all ASCII.
  UChar ored = 0;
  for (wtf_size_t i = 0; i < length; ++i) {
    UChar c = characters[i];
    result[i] = static_cast<char>(c);
    ored |= c;
  }

  if (!(ored & 0xFF80))
    return result;

  // If it wasn't all ASCII, call the function that handles more-complex cases.
  return EncodeComplexUserDefined(characters, length, handling);
}

std::string TextCodecUserDefined::Encode(const UChar* characters,
                                         wtf_size_t length,
                                         UnencodableHandling handling) {
  return EncodeCommon(characters, length, handling);
}

std::string TextCodecUserDefined::Encode(const LChar* characters,
                                         wtf_size_t length,
                                         UnencodableHandling handling) {
  return EncodeCommon(characters, length, handling);
}

}  // namespace WTF
