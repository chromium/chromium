// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/justification_opportunity.h"

#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

StringView JustificationContext::ToString(JustificationContext::Type type) {
  switch (type) {
    case Type::kNormal:
      return "kNormal";
    case Type::kAtomicInline:
      return "kAtomicInline";
    case Type::kCursive:
      return "kCursive";
  }
  return {};
}

String JustificationContext::ToString() const {
  return StrCat(
      {"JustificationContext {previous_type:", ToString(previous_type),
       ", is_after_opportunity:", String::Boolean(is_after_opportunity), "}"});
}

// Returns a pair of flags;
// - first: true if we should expand just before `ch`
// - second: true if we should expand just after `ch`
template <typename CharType>
  requires IsStringCharType<CharType>
std::pair<bool, bool> CheckJustificationOpportunity(
    TextJustify method,
    UChar32 ch,
    JustificationContext& context) {
  if (Character::IsDefaultIgnorable(ch)) {
    return {false, false};
  }
  constexpr bool kIsLatin = sizeof(CharType) == 1u;
  JustificationContext::Type type = JustificationContext::Type::kNormal;
  if constexpr (!kIsLatin) {
    if (ch == uchar::kObjectReplacementCharacter) {
      type = JustificationContext::Type::kAtomicInline;
    } else if (Character::IsCursiveScript(ch)) {
      type = JustificationContext::Type::kCursive;
    }
  }
  JustificationContext::Type previous_type = context.previous_type;
  context.previous_type = type;

  switch (method) {
    // https://drafts.csswg.org/css-text-4/#valdef-text-justify-none
    case TextJustify::kNone:
      context.is_after_opportunity = false;
      return {false, false};

    // https://drafts.csswg.org/css-text-4/#valdef-text-justify-inter-character
    case TextJustify::kInterCharacter: {
      if (type != JustificationContext::Type::kNormal) {
        // For atomic inlines and cursive scripts, we should expand before
        // the glyph if the previous character type is different from the
        // current one.
        bool expand_before =
            !context.is_after_opportunity && previous_type != type;
        // We never expand after an atomic inilne or a cursive script because
        // the next character might have the same type.
        context.is_after_opportunity = false;
        return {expand_before, false};
      }
      // We should expand before this glyph if the glyph is placed after an
      // atomic inline or a cursive script.
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

  if constexpr (kIsLatin) {
    context.is_after_opportunity = false;
    return {false, false};
  }

  // IsCJKIdeographOrSymbol() has opportunities both before and after
  // each character.
  // http://www.w3.org/TR/jlreq/#line_adjustment
  if (!Character::IsCJKIdeographOrSymbol(ch)) {
    context.is_after_opportunity = false;
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
