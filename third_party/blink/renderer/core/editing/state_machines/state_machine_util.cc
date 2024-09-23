// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/editing/state_machines/state_machine_util.h"

#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

namespace {

// The list of code points which has Indic_Syllabic_Category=Virama property.
// Must be sorted.
// See http://www.unicode.org/Public/9.0.0/ucd/IndicSyllabicCategory-9.0.0d2.txt
const uint32_t kIndicSyllabicCategoryViramaList[] = {
    // Do not include 0+0BCD TAMIL SIGN VIRAMA as Tamil works differently from
    // other Indic languages. See crbug.com/693687.
    0x094D,  0x09CD,  0x0A4D,  0x0ACD,  0x0B4D,  0x0C4D,  0x0CCD,  0x0D4D,
    0x0DCA,  0x1B44,  0xA8C4,  0xA9C0,  0x11046, 0x110B9, 0x111C0, 0x11235,
    0x1134D, 0x11442, 0x114C2, 0x115BF, 0x1163F, 0x116B6, 0x11C3F,
};

// Returns true if the code point has Indic_Syllabic_Category=Virama property.
// See http://www.unicode.org/Public/9.0.0/ucd/IndicSyllabicCategory-9.0.0d2.txt
bool IsIndicSyllabicCategoryVirama(uint32_t code_point) {
  const int length = std::size(kIndicSyllabicCategoryViramaList);
  return std::binary_search(kIndicSyllabicCategoryViramaList,
                            kIndicSyllabicCategoryViramaList + length,
                            code_point);
}

}  // namespace

bool IsGraphemeBreak(UChar32 prev_code_point, UChar32 next_code_point) {
  // The following breaking rules come from Unicode Standard Annex #29 on
  // Unicode Text Segmentation. See http://www.unicode.org/reports/tr29/
  int prev_prop =
      u_getIntPropertyValue(prev_code_point, UCHAR_GRAPHEME_CLUSTER_BREAK);
  int next_prop =
      u_getIntPropertyValue(next_code_point, UCHAR_GRAPHEME_CLUSTER_BREAK);

  // Rule1 GB1 sot ÷
  // Rule2 GB2 ÷ eot
  // Should be handled by caller.

  // Rule GB3, CR x LF
  if (prev_prop == U_GCB_CR && next_prop == U_GCB_LF)
    return false;

  // Rule GB4, (Control | CR | LF) ÷
  if (prev_prop == U_GCB_CONTROL || prev_prop == U_GCB_CR ||
      prev_prop == U_GCB_LF)
    return true;

  // Rule GB5, ÷ (Control | CR | LF)
  if (next_prop == U_GCB_CONTROL || next_prop == U_GCB_CR ||
      next_prop == U_GCB_LF)
    return true;

  // Rule GB6, L x (L | V | LV | LVT)
  if (prev_prop == U_GCB_L && (next_prop == U_GCB_L || next_prop == U_GCB_V ||
                               next_prop == U_GCB_LV || next_prop == U_GCB_LVT))
    return false;

  // Rule GB7, (LV | V) x (V | T)
  if ((prev_prop == U_GCB_LV || prev_prop == U_GCB_V) &&
      (next_prop == U_GCB_V || next_prop == U_GCB_T))
    return false;

  // Rule GB8, (LVT | T) x T
  if ((prev_prop == U_GCB_LVT || prev_prop == U_GCB_T) && next_prop == U_GCB_T)
    return false;

  // Rule GB8a
  //
  // sot   (RI RI)* RI x RI
  // [^RI] (RI RI)* RI x RI
  //                RI ÷ RI
  if (Character::IsRegionalIndicator(prev_code_point) &&
      Character::IsRegionalIndicator(next_code_point))
    NOTREACHED_IN_MIGRATION()
        << "Do not use this function for regional indicators.";

  // Rule GB9, x (Extend | ZWJ)
  // Rule GB9a, x SpacingMark
  if (next_prop == U_GCB_EXTEND ||
      next_code_point == kZeroWidthJoinerCharacter ||
      next_prop == U_GCB_SPACING_MARK)
    return false;

  // Rule GB9b, Prepend x
  if (prev_prop == U_GCB_PREPEND)
    return false;

  // Cluster Indic syllables together.
  if (IsIndicSyllabicCategoryVirama(prev_code_point) &&
      u_getIntPropertyValue(next_code_point, UCHAR_GENERAL_CATEGORY) ==
          U_OTHER_LETTER)
    return false;

  // GB11, ZWJ x Emoji
  if (prev_code_point == kZeroWidthJoinerCharacter &&
      (Character::IsEmoji(next_code_point)))
    return false;

  // GB12 for RI(Regional Indicator) is handled elsewhere because it requires
  // counting the number of consecutive RIs.

  // Rule GB999 any ÷ any
  return true;
}

}  // namespace blink
