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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/text/text_run.h"

#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"

namespace blink {

struct SameSizeAsTextRun {
  DISALLOW_NEW();
  union {
    // RAW_PTR_EXCLUSION: #union
    RAW_PTR_EXCLUSION const void* pointer;
  };
  int integer;
  uint32_t bitfields : 4;
};

ASSERT_SIZE(TextRun, SameSizeAsTextRun);

void TextRun::SetText(const String& string) {
  len_ = string.length();
  if (!len_) {
    data_.characters8 = nullptr;
    is_8bit_ = true;
    return;
  }
  is_8bit_ = string.Is8Bit();
  if (is_8bit_)
    data_.characters8 = string.Characters8();
  else
    data_.characters16 = string.Characters16();
}

String TextRun::NormalizedUTF16() const {
  const UChar* source;
  String string_for8_bit_run;
  if (Is8Bit()) {
    string_for8_bit_run = String::Make16BitFrom8BitSource(Span8());
    source = string_for8_bit_run.Characters16();
  } else {
    source = Characters16();
  }

  StringBuffer<UChar> buffer(len_);
  unsigned result_length = 0;

  bool error = false;
  unsigned position = 0;
  while (position < len_) {
    UChar32 character;
    U16_NEXT(source, position, len_, character);
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

    U16_APPEND(buffer.Characters(), result_length, len_, character, error);
    DCHECK(!error);
  }

  DCHECK(result_length <= len_);
  return String::Adopt(buffer);
}

unsigned TextRun::IndexOfSubRun(const TextRun& sub_run) const {
  if (Is8Bit() == sub_run.Is8Bit() && sub_run.Bytes() >= Bytes()) {
    size_t start_index = Is8Bit() ? sub_run.Characters8() - Characters8()
                                  : sub_run.Characters16() - Characters16();
    if (start_index + sub_run.length() <= length())
      return static_cast<unsigned>(start_index);
  }
  return std::numeric_limits<unsigned>::max();
}

void TextRun::SetDirectionFromText() {
  SetDirection(BidiParagraph::BaseDirectionForStringOrLtr(ToStringView()));
}

}  // namespace blink
