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

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/text_segments.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

namespace blink {

namespace {

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

PositionInFlatTree NextWordPositionInternal(
    const PositionInFlatTree& position) {
  class Finder final : public TextSegments::Finder {
    STACK_ALLOCATED();

   private:
    Position Find(const String text, unsigned offset) final {
      DCHECK_LE(offset, text.length());
      if (offset == text.length() || text.length() == 0)
        return Position();
      TextBreakIterator* it = WordBreakIterator(text.Span16());
      for (int runner = it->following(offset); runner != kTextBreakDone;
           runner = it->following(runner)) {
        // Accumulate punctuation runs
        if (static_cast<unsigned>(runner) < text.length() &&
            WTF::unicode::IsPunct(text[runner])) {
          if (WTF::unicode::IsAlphanumeric(text[runner - 1]))
            return Position::Before(runner);
          continue;
        }
        // We stop searching when the character preceding the break is
        // alphanumeric or punctuations or underscore.
        if (static_cast<unsigned>(runner) < text.length() &&
            (WTF::unicode::IsAlphanumeric(text[runner - 1]) ||
             (WTF::unicode::IsPunct(text[runner - 1])) ||
             text[runner - 1] == kLowLineCharacter)) {
          return Position::After(runner - 1);
        }
      }
      if (text[text.length() - 1] != kNewlineCharacter)
        return Position::After(text.length() - 1);
      return Position();
    }
  } finder;
  return TextSegments::FindBoundaryForward(position, &finder);
}

PositionInFlatTree PreviousWordPositionInternal(
    const PositionInFlatTree& position) {
  class Finder final : public TextSegments::Finder {
    STACK_ALLOCATED();

   private:
    Position Find(const String text, unsigned offset) final {
      DCHECK_LE(offset, text.length());
      if (!offset || text.length() == 0)
        return Position();
      TextBreakIterator* it = WordBreakIterator(text.Span16());
      int punct_runner = -1;
      for (int runner = it->preceding(offset); runner != kTextBreakDone;
           runner = it->preceding(runner)) {
        // Accumulate punctuation runs
        if (static_cast<unsigned>(runner) < text.length() &&
            WTF::unicode::IsPunct(text[runner])) {
          if (WTF::unicode::IsAlphanumeric(text[runner - 1]))
            return Position::Before(runner);
          punct_runner = runner;
          continue;
        }

        if (punct_runner >= 0)
          return Position::Before(punct_runner);
        // We stop searching when the character following the break is
        // alphanumeric or punctuations or underscore.
        if (runner && (WTF::unicode::IsAlphanumeric(text[runner]) ||
                       text[runner] == kLowLineCharacter))
          return Position::Before(runner);
      }
      return Position::Before(0);
    }
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

VisiblePosition EndOfWord(const VisiblePosition& position, WordSide side) {
  return CreateVisiblePosition(
      EndOfWordPosition(position.DeepEquivalent(), side),
      TextAffinity::kUpstreamIfPossible);
}

VisiblePositionInFlatTree EndOfWord(const VisiblePositionInFlatTree& position,
                                    WordSide side) {
  return CreateVisiblePosition(
      EndOfWordPosition(position.DeepEquivalent(), side),
      TextAffinity::kUpstreamIfPossible);
}

// ----
// TODO(editing-dev): Because of word boundary can not be an upstream position,
// we should make this function to return |PositionInFlatTree|.
PositionInFlatTreeWithAffinity NextWordPosition(
    const PositionInFlatTree& start) {
  const PositionInFlatTree next = NextWordPositionInternal(start);
  // Note: The word boundary can not be upstream position.
  const PositionInFlatTreeWithAffinity adjusted =
      AdjustForwardPositionToAvoidCrossingEditingBoundaries(
          PositionInFlatTreeWithAffinity(next), start);
  DCHECK_EQ(adjusted.Affinity(), TextAffinity::kDownstream);
  return adjusted;
}

PositionWithAffinity NextWordPosition(const Position& start) {
  const PositionInFlatTreeWithAffinity& next =
      NextWordPosition(ToPositionInFlatTree(start));
  return ToPositionInDOMTreeWithAffinity(next);
}

PositionInFlatTreeWithAffinity PreviousWordPosition(
    const PositionInFlatTree& start) {
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

VisiblePosition StartOfWord(const VisiblePosition& position, WordSide side) {
  return CreateVisiblePosition(
      StartOfWordPosition(position.DeepEquivalent(), side));
}

VisiblePositionInFlatTree StartOfWord(const VisiblePositionInFlatTree& position,
                                      WordSide side) {
  return CreateVisiblePosition(
      StartOfWordPosition(position.DeepEquivalent(), side));
}

}  // namespace blink
