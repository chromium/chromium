/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/text_offset_mapping.h"
#include "third_party/blink/renderer/core/editing/text_segments.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

namespace blink {

namespace {

// Helpers used during word movement
static bool IsLineBreak(UChar ch) {
  return ch == kNewlineCharacter || ch == kCarriageReturnCharacter;
}

PositionInFlatTree EndOfWordPositionInternal(const PositionInFlatTree& position,
                                             WordSide side) {
  class Finder final : public TextSegments::Finder {
    STACK_ALLOCATED();

   public:
    Finder(WordSide side) : side_(side) {}

   private:
    Position Find(const String text, unsigned offset) final {
      DCHECK_LE(offset, text.length());
      if (!is_first_time_)
        return FindInternal(text, offset);
      is_first_time_ = false;
      if (side_ == kPreviousWordIfOnBoundary) {
        if (offset == 0)
          return Position::Before(0);
        return FindInternal(text, offset - 1);
      }
      if (offset == text.length())
        return Position::After(offset);
      return FindInternal(text, offset);
    }

    static Position FindInternal(const String text, unsigned offset) {
      DCHECK_LE(offset, text.length());
      TextBreakIterator* it = WordBreakIterator(text.Span16());
      const int result = it->following(offset);
      if (result == kTextBreakDone || result == 0)
        return Position();
      return Position::After(result - 1);
    }

    const WordSide side_;
    bool is_first_time_ = true;
  } finder(side);
  return TextSegments::FindBoundaryForward(position, &finder);
}

// IMPORTANT: If you update the logic of this algorithm, please also update the
// one in `AbstractInlineTextBox::GetWordBoundariesForText`. The word offsets
// computed over there needs to stay in sync with the ones computed here in
// order for screen readers to announce the right words when using caret
// navigation (ctrl + left/right arrow).
PositionInFlatTree NextWordPositionInternal(
    const PositionInFlatTree& position,
    PlatformWordBehavior platform_word_behavior) {
  class Finder final : public TextSegments::Finder {
    STACK_ALLOCATED();

   public:
    Finder(PlatformWordBehavior platform_word_behavior)
        : platform_word_behavior_(platform_word_behavior) {}

   private:
    Position Find(const String text, unsigned offset) final {
      DCHECK_LE(offset, text.length());
      if (!is_first_time_ && static_cast<unsigned>(offset) < text.length()) {
        // These conditions check if we found a valid word break position after
        // another iteration of scanning contents from the position that was
        // passed to this function. Ex: |Hello |World|\n |foo |bar
        // When we are after World|, the first iteration of this loop after call
        // to TextSegments::Finder::find will return empty position as there
        // aren't any meaningful word in that inline_content. In the next
        // iteration of this loop, it fetches the word |foo, so we return the
        // current position as we don't want to skip this valid position by
        // advancing from this position and return |bar instead.
        if (IsWordBreak(text[offset]))
          return SkipWhitespaceIfNeeded(text, offset);
      }
      is_first_time_ = false;
      if (offset == text.length() || text.length() == 0)
        return Position();
      TextBreakIterator* it = WordBreakIterator(text.Span16());
      for (int runner = it->following(offset); runner != kTextBreakDone;
           runner = it->following(runner)) {
        // Move after line break
        if (IsLineBreak(text[runner]))
          return SkipWhitespaceIfNeeded(text, runner);
        // Accumulate punctuation/surrogate pair runs.
        if (static_cast<unsigned>(runner) < text.length() &&
            (WTF::unicode::IsPunct(text[runner]) ||
             U16_IS_SURROGATE(text[runner]))) {
          if (WTF::unicode::IsAlphanumeric(text[runner - 1]))
            return SkipWhitespaceIfNeeded(text, runner);
          continue;
        }
        // We stop searching in the following conditions:
        // 1. When the character preceding the break is
        //    alphanumeric or punctuations or underscore or linebreaks.
        // Only on Windows:
        // 2. When the character preceding the break is a whitespace and
        //    the character following it is an alphanumeric or punctuations
        //    or underscore or linebreaks.
        if (static_cast<unsigned>(runner) < text.length() &&
            IsWordBreak(text[runner - 1]))
          return SkipWhitespaceIfNeeded(text, runner);
        else if (platform_word_behavior_ ==
                     PlatformWordBehavior::kWordSkipSpaces &&
                 static_cast<unsigned>(runner) < text.length() &&
                 IsWhitespace(text[runner - 1]) && IsWordBreak(text[runner]))
          return SkipWhitespaceIfNeeded(text, runner);
      }
      if (text[text.length() - 1] != kNewlineCharacter)
        return Position::After(text.length() - 1);
      return Position();
    }

    Position SkipWhitespaceIfNeeded(const String text, int offset) {
      DCHECK_NE(offset, kTextBreakDone);
      // On Windows next word should skip trailing whitespaces but not line
      // break
      if (platform_word_behavior_ == PlatformWordBehavior::kWordSkipSpaces) {
        for (unsigned runner = static_cast<unsigned>(offset);
             runner < text.length(); ++runner) {
          if (!(IsWhitespace(text[runner]) ||
                WTF::unicode::Direction(text[runner]) ==
                    WTF::unicode::kWhiteSpaceNeutral) ||
              IsLineBreak(text[runner]))
            return Position::Before(runner);
        }
      }
      return Position::Before(offset);
    }

    const PlatformWordBehavior platform_word_behavior_;
    bool is_first_time_ = true;
  } finder(platform_word_behavior);
  return TextSegments::FindBoundaryForward(position, &finder);
}

PositionInFlatTree PreviousWordPositionInternal(
    const PositionInFlatTree& position) {
  class Finder final : public TextSegments::Finder {
    STACK_ALLOCATED();

   private:
    Position Find(const String text, unsigned offset) final {
      DCHECK_LE(offset, text.length());
      if (!is_first_time_ && text.length() > 0 &&
          static_cast<unsigned>(offset) <= text.length()) {
        // These conditions check if we found a valid word break position after
        // another iteration of scanning contents from the position that was
        // passed to this function. Ex: |Hello |World|\n |foo |bar
        // When we are before |foo, the first iteration of this loop after call
        // to TextSegments::Finder::find will return empty position as there
        // aren't any meaningful word in that inline_content. In the next
        // iteration of this loop, it fetches the word World|, so we return the
        // current position as we don't want to skip this valid position by
        // advancing from this position and return |World instead.
        if (IsWordBreak(text[offset - 1]))
          return Position::Before(offset);
      }
      is_first_time_ = false;
      if (!offset || text.length() == 0)
        return Position();
      TextBreakIterator* it = WordBreakIterator(text.Span16());
      int punct_runner = -1;
      for (int runner = it->preceding(offset); runner != kTextBreakDone;
           runner = it->preceding(runner)) {
        // Accumulate punctuation/surrogate pair runs.
        if (static_cast<unsigned>(runner) < text.length() &&
            (WTF::unicode::IsPunct(text[runner]) ||
             U16_IS_SURROGATE(text[runner]))) {
          if (WTF::unicode::IsAlphanumeric(text[runner - 1]))
            return Position::Before(runner);
          punct_runner = runner;
          continue;
        }

        if (punct_runner >= 0)
          return Position::Before(punct_runner);
        // We stop searching when the character following the break is
        // alphanumeric or punctuations or underscore or linebreaks.
        if (static_cast<unsigned>(runner) < text.length() &&
            IsWordBreak(text[runner]))
          return Position::Before(runner);
      }
      return Position::Before(0);
    }
    bool is_first_time_ = true;
  } finder;
  return TextSegments::FindBoundaryBackward(position, &finder);
}

PositionInFlatTree StartOfWordPositionInternal(
    const PositionInFlatTree& position,
    WordSide side) {
  class Finder final : public TextSegments::Finder {
    STACK_ALLOCATED();

   public:
    Finder(WordSide side) : side_(side) {}

   private:
    Position Find(const String text, unsigned offset) final {
      DCHECK_LE(offset, text.length());
      if (!is_first_time_)
        return FindInternal(text, offset);
      is_first_time_ = false;
      if (side_ == kNextWordIfOnBoundary) {
        if (offset == text.length())
          return Position::After(text.length());
        return FindInternal(text, offset + 1);
      }
      if (!offset)
        return Position::Before(offset);
      return FindInternal(text, offset);
    }

    static Position FindInternal(const String text, unsigned offset) {
      DCHECK_LE(offset, text.length());
      TextBreakIterator* it = WordBreakIterator(text.Span16());
      const int result = it->preceding(offset);
      if (result == kTextBreakDone)
        return Position();
      return Position::Before(result);
    }

    const WordSide side_;
    bool is_first_time_ = true;
  } finder(side);
  return TextSegments::FindBoundaryBackward(position, &finder);
}
}  // namespace

PositionInFlatTree EndOfWordPosition(const PositionInFlatTree& start,
                                     WordSide side) {
  return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
             PositionInFlatTreeWithAffinity(
                 EndOfWordPositionInternal(start, side)),
             start)
      .GetPosition();
}

Position EndOfWordPosition(const Position& position, WordSide side) {
  return ToPositionInDOMTree(
      EndOfWordPosition(ToPositionInFlatTree(position), side));
}

// ----
// TODO(editing-dev): Because of word boundary can not be an upstream position,
// we should make this function to return |PositionInFlatTree|.
PositionInFlatTreeWithAffinity NextWordPosition(
    const PositionInFlatTree& start,
    PlatformWordBehavior platform_word_behavior) {
  if (start.IsNull())
    return PositionInFlatTreeWithAffinity();
  const PositionInFlatTree next =
      NextWordPositionInternal(start, platform_word_behavior);
  // Note: The word boundary can not be upstream position.
  const PositionInFlatTreeWithAffinity adjusted =
      AdjustForwardPositionToAvoidCrossingEditingBoundaries(
          PositionInFlatTreeWithAffinity(next), start);
  DCHECK_EQ(adjusted.Affinity(), TextAffinity::kDownstream);
  return adjusted;
}

PositionWithAffinity NextWordPosition(
    const Position& start,
    PlatformWordBehavior platform_word_behavior) {
  const PositionInFlatTreeWithAffinity& next =
      NextWordPosition(ToPositionInFlatTree(start), platform_word_behavior);
  return ToPositionInDOMTreeWithAffinity(next);
}

PositionInFlatTreeWithAffinity PreviousWordPosition(
    const PositionInFlatTree& start) {
  if (start.IsNull())
    return PositionInFlatTreeWithAffinity();
  const PositionInFlatTree prev = PreviousWordPositionInternal(start);
  return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
      PositionInFlatTreeWithAffinity(prev), start);
}

PositionWithAffinity PreviousWordPosition(const Position& start) {
  const PositionInFlatTreeWithAffinity& prev =
      PreviousWordPosition(ToPositionInFlatTree(start));
  return ToPositionInDOMTreeWithAffinity(prev);
}

PositionInFlatTree StartOfWordPosition(const PositionInFlatTree& position,
                                       WordSide side) {
  const PositionInFlatTree start = StartOfWordPositionInternal(position, side);
  return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
             PositionInFlatTreeWithAffinity(start), position)
      .GetPosition();
}

Position StartOfWordPosition(const Position& position, WordSide side) {
  return ToPositionInDOMTree(
      StartOfWordPosition(ToPositionInFlatTree(position), side));
}

PositionInFlatTree MiddleOfWordPosition(const PositionInFlatTree& word_start,
                                        const PositionInFlatTree& word_end) {
  if (word_start >= word_end) {
    return PositionInFlatTree(nullptr, 0);
  }
  unsigned middle =
      TextIteratorAlgorithm<EditingInFlatTreeStrategy>::RangeLength(word_start,
                                                                    word_end) /
      2;
  TextOffsetMapping::ForwardRange range =
      TextOffsetMapping::ForwardRangeOf(word_start);
  middle += TextOffsetMapping(*range.begin()).ComputeTextOffset(word_start);
  for (auto inline_contents : range) {
    const TextOffsetMapping mapping(inline_contents);
    unsigned length = mapping.GetText().length();
    if (middle < length) {
      return mapping.GetPositionBefore(middle);
    }
    middle -= length;
  }
  NOTREACHED_IN_MIGRATION();
  return PositionInFlatTree(nullptr, 0);
}

Position MiddleOfWordPosition(const Position& word_start,
                              const Position& word_end) {
  return ToPositionInDOMTree(MiddleOfWordPosition(
      ToPositionInFlatTree(word_start), ToPositionInFlatTree(word_end)));
}

bool IsWordBreak(UChar ch) {
  return (WTF::unicode::IsPrintableChar(ch) && !IsWhitespace(ch)) ||
         U16_IS_SURROGATE(ch) || IsLineBreak(ch) || ch == kLowLineCharacter;
}
}  // namespace blink
