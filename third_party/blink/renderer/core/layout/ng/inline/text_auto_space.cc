// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/text_auto_space.h"

#include <unicode/uchar.h>
#include <unicode/uscript.h>

#include "base/check.h"

namespace blink {

// static
TextAutoSpace::CharType TextAutoSpace::GetType(UChar32 ch) {
  // This logic is based on:
  // https://drafts.csswg.org/css-text-4/#text-spacing-classes
  const uint32_t gc_mask = U_GET_GC_MASK(ch);
  if (ch >= 0x3041 && ch <= 0x30FF && !(gc_mask & U_GC_P_MASK)) {
    return kIdeograph;
  }
  if (ch >= 0x31C0 && ch <= 0x31FF) {
    return kIdeograph;
  }
  UErrorCode err = U_ZERO_ERROR;
  const UScriptCode script = uscript_getScript(ch, &err);
  DCHECK(U_SUCCESS(err));
  if (U_SUCCESS(err) && script == USCRIPT_HAN) {
    return kIdeograph;
  }

  if (gc_mask & (U_GC_L_MASK | U_GC_M_MASK | U_GC_ND_MASK)) {
    const UEastAsianWidth eaw = static_cast<UEastAsianWidth>(
        u_getIntPropertyValue(ch, UCHAR_EAST_ASIAN_WIDTH));
    if (eaw != UEastAsianWidth::U_EA_FULLWIDTH) {
      return kLetterOrNumeral;
    }
  }
  return kOther;
}

std::ostream& operator<<(std::ostream& ostream, TextAutoSpace::CharType type) {
  switch (type) {
    case TextAutoSpace::kIdeograph:
      return ostream << "kIdeograph";
    case TextAutoSpace::kLetterOrNumeral:
      return ostream << "kLetterOrNumeral";
    case TextAutoSpace::kOther:
      return ostream << "kOther";
  }
  return ostream << static_cast<int>(type);
}

}  // namespace blink
