/*
 * Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_BASE64_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_BASE64_H_

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

enum Base64EncodePolicy { kBase64DoNotInsertLFs, kBase64InsertLFs };

enum Base64DecodePolicy { kBase64DoNotValidatePadding, kBase64ValidatePadding };

WTF_EXPORT void Base64Encode(base::span<const uint8_t>,
                             Vector<char>&,
                             Base64EncodePolicy = kBase64DoNotInsertLFs);
WTF_EXPORT String Base64Encode(base::span<const uint8_t>,
                               Base64EncodePolicy = kBase64DoNotInsertLFs)
    WARN_UNUSED_RESULT;

WTF_EXPORT bool Base64Decode(
    const String&,
    Vector<char>&,
    CharacterMatchFunctionPtr should_ignore_character = nullptr,
    Base64DecodePolicy = kBase64DoNotValidatePadding);
WTF_EXPORT bool Base64Decode(
    const Vector<char>&,
    Vector<char>&,
    CharacterMatchFunctionPtr should_ignore_character = nullptr,
    Base64DecodePolicy = kBase64DoNotValidatePadding);
WTF_EXPORT bool Base64Decode(
    const char*,
    unsigned,
    Vector<char>&,
    CharacterMatchFunctionPtr should_ignore_character = nullptr,
    Base64DecodePolicy = kBase64DoNotValidatePadding);
WTF_EXPORT bool Base64Decode(
    const UChar*,
    unsigned,
    Vector<char>&,
    CharacterMatchFunctionPtr should_ignore_character = nullptr,
    Base64DecodePolicy = kBase64DoNotValidatePadding);
WTF_EXPORT bool Base64UnpaddedURLDecode(
    const String& in,
    Vector<char>&,
    CharacterMatchFunctionPtr should_ignore_character = nullptr,
    Base64DecodePolicy = kBase64DoNotValidatePadding);

// Given an encoding in either base64 or base64url, returns a normalized
// encoding in plain base64.
WTF_EXPORT String NormalizeToBase64(const String&);

WTF_EXPORT String Base64URLEncode(const char*,
                                  unsigned,
                                  Base64EncodePolicy = kBase64DoNotInsertLFs);

}  // namespace WTF

using WTF::Base64EncodePolicy;
using WTF::kBase64DoNotInsertLFs;
using WTF::kBase64InsertLFs;
using WTF::Base64DecodePolicy;
using WTF::kBase64DoNotValidatePadding;
using WTF::kBase64ValidatePadding;
using WTF::Base64Encode;
using WTF::Base64Decode;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_BASE64_H_
