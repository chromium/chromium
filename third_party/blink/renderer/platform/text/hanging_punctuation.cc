// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/hanging_punctuation.h"

#include <unicode/uchar.h>

namespace blink {

bool IsHangingPunctuation(UChar32 ch, HangingPunctuation mask) {
  // Determine if it is a hanging punctuation character or not based on
  // https://drafts.csswg.org/css-text/#hanging-punctuation-property
  switch (mask) {
    case HangingPunctuation::kFirst: {
      if (ch == 0x0022 ||  // QUOTATION MARK
          ch == 0x0027 ||  // APOSTROPHE
          ch == 0x3000     // IDEOGRAPHIC SPACE
      ) {
        return true;
      }
      UCharCategory category = static_cast<UCharCategory>(u_charType(ch));
      return category == U_START_PUNCTUATION ||
             category == U_INITIAL_PUNCTUATION ||
             category == U_FINAL_PUNCTUATION;
    }
    case HangingPunctuation::kLast: {
      if (ch == 0x0022 ||  // QUOTATION MARK
          ch == 0x0027     // APOSTROPHE
      ) {
        return true;
      }
      UCharCategory category = static_cast<UCharCategory>(u_charType(ch));
      return category == U_END_PUNCTUATION ||
             category == U_INITIAL_PUNCTUATION ||
             category == U_FINAL_PUNCTUATION;
    }
    case HangingPunctuation::kAllowEnd:
      switch (ch) {
        case 0x002C:  // , COMMA
        case 0x002E:  // . FULL STOP
        case 0x060C:  // ، ARABIC COMMA
        case 0x06D4:  // ۔ ARABIC FULL STOP
        case 0x3001:  // 、 IDEOGRAPHIC COMMA
        case 0x3002:  // 。 IDEOGRAPHIC FULL STOP
        case 0xFF0C:  // ， FULLWIDTH COMMA
        case 0xFF0E:  // ． FULLWIDTH FULL STOP
        case 0xFE50:  // ﹐ SMALL COMMA
        case 0xFE51:  // ﹑ SMALL IDEOGRAPHIC COMMA
        case 0xFE52:  // ﹒ SMALL FULL STOP
        case 0xFF61:  // ｡ HALFWIDTH IDEOGRAPHIC FULL STOP
        case 0xFF64:  // ､ HALFWIDTH IDEOGRAPHIC COMMA
          return true;
        default:
          return false;
      }
    default:
      return false;
  }
}

}  // namespace blink
