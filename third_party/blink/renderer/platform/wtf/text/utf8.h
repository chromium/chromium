/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UTF8_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UTF8_H_

#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {
namespace unicode {

typedef enum {
  kConversionOK,     // conversion successful
  kSourceExhausted,  // partial character in source, but hit end
  kTargetExhausted,  // insuff. room in target for conversion
  kSourceIllegal     // source sequence is illegal/malformed
} ConversionResult;

// These conversion functions take a "strict" argument. When this flag is set to
// true (i.e. strict), both irregular sequences and isolated surrogates will
// cause an error.  When the flag is set to false (i.e. lenient), both irregular
// sequences and isolated surrogates are converted.
//
// Whether the flag is strict or lenient, all illegal sequences will cause an
// error return. This includes sequences such as: <F4 90 80 80>, <C0 80>, or
// <A0> in UTF-8, and values above 0x10FFFF in UTF-32. Conformant code must
// check for illegal sequences.
//
// When the flag is set to lenient, characters over 0x10FFFF are converted to
// the replacement character; otherwise (when the flag is set to strict) they
// constitute an error.
// TODO(crbug.com/329702346): It is not clear how characters over 0x10FFFF can
// be represented in the encodings that these functions claim to handle. In
// UTF-16, surrogate pairs should not be able to encode codepoints higher than
// 0x10FFFF; in UTF-8, the 4-byte form is similarly unable to encode codepoints
// higher than 0x10FFFF.

WTF_EXPORT ConversionResult ConvertUTF8ToUTF16(const char** source_start,
                                               const char* source_end,
                                               UChar** target_start,
                                               UChar* target_end,
                                               bool strict = true);

WTF_EXPORT ConversionResult ConvertLatin1ToUTF8(const LChar** source_start,
                                                const LChar* source_end,
                                                char** target_start,
                                                char* target_end);

WTF_EXPORT ConversionResult ConvertUTF16ToUTF8(const UChar** source_start,
                                               const UChar* source_end,
                                               char** target_start,
                                               char* target_end,
                                               bool strict = true);

// Returns the number of UTF-16 code points.
WTF_EXPORT unsigned CalculateStringLengthFromUTF8(const char* data,
                                                  const char*& data_end,
                                                  bool& seen_non_ascii,
                                                  bool& seen_non_latin1);

WTF_EXPORT bool EqualUTF16WithUTF8(const UChar* a,
                                   const UChar* a_end,
                                   const char* b,
                                   const char* b_end);
WTF_EXPORT bool EqualLatin1WithUTF8(const LChar* a,
                                    const LChar* a_end,
                                    const char* b,
                                    const char* b_end);

}  // namespace unicode
}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UTF8_H_
