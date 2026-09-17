// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_WTF_UCHAR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_WTF_UCHAR_H_

#include <type_traits>

#if defined(USING_SYSTEM_ICU)

#include <unicode/umachine.h>  // IWYU pragma: export

#else

#include <stdint.h>

// These definitions should be matched to
// third_party/icu/source/common/unicode/umachine.h.
using UChar = char16_t;
using UChar32 = int32_t;

#endif

static_assert(sizeof(UChar) == 2, "UChar should be two bytes");

namespace blink {

// Define platform neutral 8 bit character type (L is for Latin-1).
using LChar = unsigned char;

// A concept to check if a type is LChar or UChar.
template <typename CharType>
concept IsStringCharType =
    std::is_same_v<LChar, CharType> || std::is_same_v<UChar, CharType>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_WTF_UCHAR_H_
