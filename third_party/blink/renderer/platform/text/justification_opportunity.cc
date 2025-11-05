// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/justification_opportunity.h"

#include "third_party/blink/renderer/platform/text/character.h"

namespace blink {

// Returns a pair of flags;
// - first: true if we should expand just before `ch`
// - second: true if we should expand just after `ch`
template <typename CharType>
  requires IsStringCharType<CharType>
std::pair<bool, bool> CheckJustificationOpportunity(
    TextJustify method,
    UChar32 ch,
    bool& is_after_opportunity) {
  bool treat_as_space = Character::TreatAsSpace(ch);
  if (treat_as_space && ch != uchar::kNoBreakSpace) {
    ch = uchar::kSpace;
  }

  if (treat_as_space) {
    is_after_opportunity = true;
    return {false, true};
  }

  if constexpr (sizeof(CharType) == 1u) {
    is_after_opportunity = false;
    return {false, false};
  }

  // IsCJKIdeographOrSymbol() has opportunities both before and after
  // each character.
  // http://www.w3.org/TR/jlreq/#line_adjustment
  if (!Character::IsCJKIdeographOrSymbol(ch)) {
    if (!Character::IsDefaultIgnorable(ch)) {
      is_after_opportunity = false;
    }
    return {false, false};
  }

  // We won't expand before this character if
  //  - We expand after the previous character, or
  //  - The character is at the beginning of a text.
  bool expand_before = !is_after_opportunity;
  is_after_opportunity = true;
  return {expand_before, true};
}

std::pair<bool, bool> CheckJustificationOpportunity8(
    TextJustify method,
    LChar ch,
    bool& is_after_opportunity) {
  return CheckJustificationOpportunity<LChar>(method, ch, is_after_opportunity);
}

std::pair<bool, bool> CheckJustificationOpportunity16(
    TextJustify method,
    UChar32 ch,
    bool& is_after_opportunity) {
  return CheckJustificationOpportunity<UChar>(method, ch, is_after_opportunity);
}

}  // namespace blink
