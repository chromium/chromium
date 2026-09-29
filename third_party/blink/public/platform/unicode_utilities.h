// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_UNICODE_UTILITIES_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_UNICODE_UTILITIES_H_

#include <string_view>

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// Returns if `pattern` contains any Japanese kana letters.
BLINK_PLATFORM_EXPORT bool ContainsKanaLetters(std::u16string_view pattern);

// Checks that all Japanese kana letters in `search_text` and `match` match in
// size (small vs. normal) and voicing marks (dakuten/handakuten), ignoring any
// non-kana characters. Intended to refine primary-strength ICU search matches,
// which ignore kana size and voicing differences. Assumes both parameters are
// in NFC form, or both are in NFD form.
BLINK_PLATFORM_EXPORT bool CheckOnlyKanaLettersInStrings(
    std::u16string_view search_text,
    std::u16string_view match);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_UNICODE_UTILITIES_H_
