/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Tony Chang <idealisms@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/commands/smart_replace.h"

#include "build/build_config.h"

#if !defined(OS_MACOSX)
#include <unicode/uset.h>
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

static void AddAllCodePoints(USet* smart_set, const String& string) {
  for (wtf_size_t i = 0; i < string.length(); i++)
    uset_add(smart_set, string[i]);
}

// This is mostly a port of the code in
// core/editing/commands/smart_replace_cf.cc except we use icu in place of
// CoreFoundations character classes.
static USet* GetSmartSet(bool is_previous_character) {
  static USet* pre_smart_set = nullptr;
  static USet* post_smart_set = nullptr;
  USet* smart_set = is_previous_character ? pre_smart_set : post_smart_set;
  if (!smart_set) {
    // Whitespace and newline (kCFCharacterSetWhitespaceAndNewline)
    static const UChar* kWhitespaceAndNewLine = reinterpret_cast<const UChar*>(
        u"[[:WSpace:] [\\u000A\\u000B\\u000C\\u000D\\u0085]]");
    UErrorCode ec = U_ZERO_ERROR;
    smart_set = uset_openPattern(
        kWhitespaceAndNewLine,
        LengthOfNullTerminatedString(kWhitespaceAndNewLine), &ec);
    DCHECK(U_SUCCESS(ec)) << ec;

    // CJK ranges
    uset_addRange(smart_set, 0x1100,
                  0x1100 + 256);  // Hangul Jamo (0x1100 - 0x11FF)
    uset_addRange(smart_set, 0x2E80,
                  0x2E80 + 352);  // CJK & Kangxi Radicals (0x2E80 - 0x2FDF)
    // Ideograph Descriptions, CJK Symbols, Hiragana, Katakana, Bopomofo, Hangul
    // Compatibility Jamo, Kanbun, & Bopomofo Ext (0x2FF0 - 0x31BF)
    uset_addRange(smart_set, 0x2FF0, 0x2FF0 + 464);
    // Enclosed CJK, CJK Ideographs (Uni Han & Ext A), & Yi (0x3200 - 0xA4CF)
    uset_addRange(smart_set, 0x3200, 0x3200 + 29392);
    uset_addRange(smart_set, 0xAC00,
                  0xAC00 + 11183);  // Hangul Syllables (0xAC00 - 0xD7AF)
    uset_addRange(
        smart_set, 0xF900,
        0xF900 + 352);  // CJK Compatibility Ideographs (0xF900 - 0xFA5F)
    uset_addRange(smart_set, 0xFE30,
                  0xFE30 + 32);  // CJK Compatibility From (0xFE30 - 0xFE4F)
    uset_addRange(smart_set, 0xFF00,
                  0xFF00 + 240);  // Half/Full Width Form (0xFF00 - 0xFFEF)
    uset_addRange(smart_set, 0x20000,
                  0x20000 + 0xA6D7);  // CJK Ideograph Exntension B
    uset_addRange(
        smart_set, 0x2F800,
        0x2F800 + 0x021E);  // CJK Compatibility Ideographs (0x2F800 - 0x2FA1D)

    if (is_previous_character) {
      AddAllCodePoints(smart_set, "([\"\'#$/-`{");
      pre_smart_set = smart_set;
    } else {
      AddAllCodePoints(smart_set, ")].,;:?\'!\"%*-/}");

      // Punctuation (kCFCharacterSetPunctuation)
      static const UChar* kPunctuationClass =
          reinterpret_cast<const UChar*>(u"[:P:]");
      UErrorCode ec = U_ZERO_ERROR;
      USet* icu_punct = uset_openPattern(
          kPunctuationClass, LengthOfNullTerminatedString(kPunctuationClass),
          &ec);
      DCHECK(U_SUCCESS(ec)) << ec;
      uset_addAll(smart_set, icu_punct);
      uset_close(icu_punct);

      post_smart_set = smart_set;
    }
  }
  return smart_set;
}

bool IsCharacterSmartReplaceExempt(UChar32 c, bool is_previous_character) {
  return uset_contains(GetSmartSet(is_previous_character), c);
}
}

#endif  // !defined(OS_MACOSX)
