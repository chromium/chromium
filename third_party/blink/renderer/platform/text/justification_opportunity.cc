// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/justification_opportunity.h"

#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

// Returns a pair of flags;
// - first: true if we should expand just before `ch`
// - second: true if we should expand just after `ch`
template <typename CharType>
  requires IsStringCharType<CharType>
std::pair<bool, bool> CheckJustificationOpportunity(
    TextJustify method,
    UChar32 ch,
    JustificationContext& context) {
  switch (method) {
    // https://drafts.csswg.org/css-text-4/#valdef-text-justify-none
    case TextJustify::kNone:
      context.is_after_opportunity = false;
      return {false, false};

    // https://drafts.csswg.org/css-text-4/#valdef-text-justify-inter-character
    case TextJustify::kInterCharacter: {
      if (Character::IsDefaultIgnorable(ch)) {
        return {false, false};
      }
      if (ch == uchar::kObjectReplacementCharacter) {
        context.is_after_opportunity = false;
        return {false, false};
      }
      // We should expand before this glyph if the glyph is placed after an
      // atomic inline.
      bool expand_before = !context.is_after_opportunity;
      context.is_after_opportunity = true;
      return {expand_before, true};
    }

    // https://drafts.csswg.org/css-text-4/#valdef-text-justify-inter-word
    case TextJustify::kInterWord:
      if (Character::TreatAsSpace(ch)) {
        context.is_after_opportunity = true;
        return {false, true};
      }
      if (Character::IsDefaultIgnorable(ch)) {
        return {false, false};
      }
      context.is_after_opportunity = false;
      return {false, false};

    // https://drafts.csswg.org/css-text-4/#valdef-text-justify-auto
    case TextJustify::kAuto:
      // See below.
      break;
  }
  bool treat_as_space = Character::TreatAsSpace(ch);
  if (treat_as_space && ch != uchar::kNoBreakSpace) {
    ch = uchar::kSpace;
  }

  if (treat_as_space) {
    context.is_after_opportunity = true;
    return {false, true};
  }

  if constexpr (sizeof(CharType) == 1u) {
    context.is_after_opportunity = false;
    return {false, false};
  }

  // IsCJKIdeographOrSymbol() has opportunities both before and after
  // each character.
  // http://www.w3.org/TR/jlreq/#line_adjustment
  if (!Character::IsCJKIdeographOrSymbol(ch)) {
    if (!Character::IsDefaultIgnorable(ch)) {
      context.is_after_opportunity = false;
    }
    return {false, false};
  }

  // We won't expand before this character if
  //  - We expand after the previous character, or
  //  - The character is at the beginning of a text.
  bool expand_before = !context.is_after_opportunity;
  context.is_after_opportunity = true;
  return {expand_before, true};
}

std::pair<bool, bool> CheckJustificationOpportunity8(
    TextJustify method,
    LChar ch,
    JustificationContext& context) {
  return CheckJustificationOpportunity<LChar>(method, ch, context);
}

std::pair<bool, bool> CheckJustificationOpportunity16(
    TextJustify method,
    UChar32 ch,
    JustificationContext& context) {
  return CheckJustificationOpportunity<UChar>(method, ch, context);
}

}  // namespace blink
