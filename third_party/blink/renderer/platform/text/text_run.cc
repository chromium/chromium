/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/text/text_run.h"

#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"

namespace blink {

String TextRun::NormalizedUTF16() const {
  const UChar* source;
  String string_for8_bit_run;
  if (Is8Bit()) {
    string_for8_bit_run = String::Make16BitFrom8BitSource(Span8());
    source = string_for8_bit_run.Span16().data();
  } else {
    source = Span16().data();
  }

  wtf_size_t len = length();
  StringBuffer<UChar> buffer(len);
  unsigned result_length = 0;

  bool error = false;
  unsigned position = 0;
  while (position < len) {
    UChar32 character;
    UNSAFE_TODO(U16_NEXT(source, position, len, character));
    // Don't normalize tabs as they are not treated as spaces for word-end.
    if (NormalizeSpace() &&
        Character::IsNormalizedCanvasSpaceCharacter(character)) {
      character = kSpaceCharacter;
    } else if (Character::TreatAsSpace(character) &&
               character != kNoBreakSpaceCharacter) {
      character = kSpaceCharacter;
    } else if (Character::TreatAsZeroWidthSpaceInComplexScriptLegacy(
                   character)) {
      // Repalce only ZWS-like characters in BMP because we'd like to avoid
      // changing the string length.
      DCHECK_LT(character, 0x10000);
      character = kZeroWidthSpaceCharacter;
    }

    UNSAFE_TODO(
        U16_APPEND(buffer.Characters(), result_length, len, character, error));
    DCHECK(!error);
  }

  DCHECK(result_length <= len);
  return String::Adopt(buffer);
}

unsigned TextRun::IndexOfSubRun(const TextRun& sub_run) const {
  if (Is8Bit() == sub_run.Is8Bit() && sub_run.text_.Bytes() >= text_.Bytes()) {
    size_t start_index = Is8Bit() ? sub_run.Span8().data() - Span8().data()
                                  : sub_run.Span16().data() - Span16().data();
    if (start_index + sub_run.length() <= length())
      return static_cast<unsigned>(start_index);
  }
  return std::numeric_limits<unsigned>::max();
}

}  // namespace blink
