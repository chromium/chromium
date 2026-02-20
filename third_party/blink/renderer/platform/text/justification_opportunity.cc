// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/justification_opportunity.h"

#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/character_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/utf16.h"

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
      {"JustificationContext {previous_type_:", ToString(previous_type_),
       ", is_after_opportunity_:", String::Boolean(is_after_opportunity_),
       "}"});
}

// Returns a pair of flags;
// - first: true if we should expand just before `ch`
// - second: true if we should expand just after `ch`
template <typename CharType>
  requires IsStringCharType<CharType>
std::pair<bool, bool> JustificationContext::CheckOpportunity(TextJustify method,
                                                             UChar32 ch) {
  if (Character::IsDefaultIgnorable(ch)) {
    return {false, false};
  }
  constexpr bool kIsLatin = sizeof(CharType) == 1u;
  Type type = Type::kNormal;
  if constexpr (!kIsLatin) {
    if (ch == uchar::kObjectReplacementCharacter) {
      type = Type::kAtomicInline;
    } else if (Character::IsCursiveScript(ch)) {
      type = Type::kCursive;
    }
  }
  Type previous_type = previous_type_;
  previous_type_ = type;

  switch (method) {
    // https://drafts.csswg.org/css-text-4/#valdef-text-justify-none
    case TextJustify::kNone:
      is_after_opportunity_ = false;
      return {false, false};

    // https://drafts.csswg.org/css-text-4/#valdef-text-justify-inter-character
    case TextJustify::kInterCharacter: {
      if (type != JustificationContext::Type::kNormal) {
        // For atomic inlines and cursive scripts, we should expand before
        // the glyph if the previous character type is different from the
        // current one.
        bool expand_before = !is_after_opportunity_ && previous_type != type;
        // We never expand after an atomic inilne or a cursive script because
        // the next character might have the same type.
        is_after_opportunity_ = false;
        return {expand_before, false};
      }
      // We should expand before this glyph if the glyph is placed after an
      // atomic inline or a cursive script.
      bool expand_before = !is_after_opportunity_;
      is_after_opportunity_ = true;
      return {expand_before, true};
    }

    // https://drafts.csswg.org/css-text-4/#valdef-text-justify-inter-word
    case TextJustify::kInterWord:
      if (Character::TreatAsSpace(ch)) {
        is_after_opportunity_ = true;
        return {false, true};
      }
      is_after_opportunity_ = false;
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
    is_after_opportunity_ = true;
    return {false, true};
  }

  if constexpr (kIsLatin) {
    is_after_opportunity_ = false;
    return {false, false};
  }

  // IsCJKIdeographOrSymbol() has opportunities both before and after
  // each character.
  // http://www.w3.org/TR/jlreq/#line_adjustment
  if (!Character::IsCJKIdeographOrSymbol(ch)) {
    is_after_opportunity_ = false;
    return {false, false};
  }

  // We won't expand before this character if
  //  - We expand after the previous character, or
  //  - The character is at the beginning of a text.
  bool expand_before = !is_after_opportunity_;
  is_after_opportunity_ = true;
  return {expand_before, true};
}

std::pair<bool, bool> JustificationContext::CheckOpportunity8(
    TextJustify method,
    LChar ch) {
  return CheckOpportunity<LChar>(method, ch);
}

std::pair<bool, bool> JustificationContext::CheckOpportunity16(
    TextJustify method,
    UChar32 ch) {
  return CheckOpportunity<UChar>(method, ch);
}

wtf_size_t JustificationContext::CountOpportunities(
    TextJustify method,
    base::span<const LChar> chars,
    TextDirection direction) {
  wtf_size_t count = 0;
  if (direction == TextDirection::kLtr) {
    for (LChar ch : chars) {
      count += CountOpportunity8(method, ch);
    }
  } else {
    for (size_t i = chars.size(); i > 0; --i) {
      count += CountOpportunity8(method, chars[i - 1]);
    }
  }

  return count;
}

wtf_size_t JustificationContext::CountOpportunities(
    TextJustify method,
    base::span<const UChar> chars,
    TextDirection direction) {
  if (chars.size() == 0) {
    return 0;
  }
  wtf_size_t count = 0;

  CharacterBreakIterator iter(chars);
  if (direction == TextDirection::kLtr) {
    for (int i = 0; static_cast<size_t>(i) < chars.size(); i = iter.Next()) {
      count += CountOpportunity16(method, CodePointAt(chars, i));
    }
  } else {
    for (int i = iter.Preceding(chars.size()); i != kTextBreakDone;
         i = iter.Preceding(i)) {
      count += CountOpportunity16(method, CodePointAt(chars, i));
    }
  }
  return count;
}

}  // namespace blink
