// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/shaping/text_auto_space.h"

#include <unicode/uchar.h>
#include <unicode/uscript.h>

#include "base/check.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

float TextAutoSpace::GetSpacingWidth(const Font* font) {
  if (const SimpleFontData* font_data = font->PrimaryFont()) {
    return font_data->IdeographicInlineSize().value_or(
               font_data->PlatformData().size()) /
           8;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

// static
TextAutoSpace::CharType TextAutoSpace::GetTypeAndNext(const String& text,
                                                      wtf_size_t& offset) {
  CHECK(!text.Is8Bit());
  UChar32 ch;
  U16_NEXT(text.Characters16(), offset, text.length(), ch);
  return GetType(ch);
}

// static
TextAutoSpace::CharType TextAutoSpace::GetPrevType(const String& text,
                                                   wtf_size_t offset) {
  DCHECK_GT(offset, 0u);
  CHECK(!text.Is8Bit());
  UChar32 last_ch;
  U16_PREV(text.Characters16(), 0, offset, last_ch);
  return GetType(last_ch);
}

// static
TextAutoSpace::CharType TextAutoSpace::GetType(UChar32 ch) {
  // This logic is based on:
  // https://drafts.csswg.org/css-text-4/#text-spacing-classes
  const uint32_t gc_mask = U_GET_GC_MASK(ch);
  static_assert(kNonHanIdeographMin <= 0x30FF && 0x30FF <= kNonHanIdeographMax);
  if (ch >= kNonHanIdeographMin && ch <= 0x30FF && !(gc_mask & U_GC_P_MASK)) {
    return kIdeograph;
  }
  static_assert(kNonHanIdeographMin <= 0x31C0 && 0x31C0 <= kNonHanIdeographMax);
  if (ch >= 0x31C0 && ch <= kNonHanIdeographMax) {
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
