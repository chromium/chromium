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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_UNITS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_UNITS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/editing_boundary.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/position.h"

namespace blink {

enum class PlatformWordBehavior { kWordSkipSpaces, kWordDontSkipSpaces };
enum class SentenceTrailingSpaceBehavior { kIncludeSpace, kOmitSpace };

// ----- words -----
// Definitions are in position_units_word.cc.
CORE_EXPORT Position StartOfWordPosition(const Position&,
                                         WordSide = kNextWordIfOnBoundary);
CORE_EXPORT PositionInFlatTree
StartOfWordPosition(const PositionInFlatTree&,
                    WordSide = kNextWordIfOnBoundary);
CORE_EXPORT Position MiddleOfWordPosition(const Position&, const Position&);
CORE_EXPORT PositionInFlatTree MiddleOfWordPosition(const PositionInFlatTree&,
                                                    const PositionInFlatTree&);
CORE_EXPORT Position EndOfWordPosition(const Position&,
                                       WordSide = kNextWordIfOnBoundary);
CORE_EXPORT PositionInFlatTree
EndOfWordPosition(const PositionInFlatTree&, WordSide = kNextWordIfOnBoundary);
CORE_EXPORT PositionWithAffinity PreviousWordPosition(const Position&);
CORE_EXPORT PositionInFlatTreeWithAffinity
PreviousWordPosition(const PositionInFlatTree&);
CORE_EXPORT PositionWithAffinity NextWordPosition(
    const Position&,
    PlatformWordBehavior = PlatformWordBehavior::kWordDontSkipSpaces);
CORE_EXPORT PositionInFlatTreeWithAffinity NextWordPosition(
    const PositionInFlatTree&,
    PlatformWordBehavior = PlatformWordBehavior::kWordDontSkipSpaces);
bool IsWordBreak(UChar);
bool IsWordBoundary(UChar);

// ----- sentences -----
// Definitions are in position_units_sentence.cc.
// Note: EndOfSentence returns PositionWithAffinity (not bare Position) because
// affinity is semantically relevant at sentence boundaries, unlike other
// boundary functions. The *Position suffix is therefore omitted.
CORE_EXPORT Position StartOfSentencePosition(const Position&);
CORE_EXPORT PositionInFlatTree
StartOfSentencePosition(const PositionInFlatTree&);
CORE_EXPORT PositionWithAffinity
EndOfSentence(const Position&,
              SentenceTrailingSpaceBehavior =
                  SentenceTrailingSpaceBehavior::kIncludeSpace);
CORE_EXPORT PositionInFlatTreeWithAffinity
EndOfSentence(const PositionInFlatTree&,
              SentenceTrailingSpaceBehavior =
                  SentenceTrailingSpaceBehavior::kIncludeSpace);
CORE_EXPORT PositionInFlatTree
PreviousSentencePosition(const PositionInFlatTree&);
CORE_EXPORT PositionInFlatTree NextSentencePosition(const PositionInFlatTree&);
EphemeralRange ExpandEndToSentenceBoundary(const EphemeralRange&);
EphemeralRange ExpandRangeToSentenceBoundary(const EphemeralRange&);

// ----- document -----
CORE_EXPORT Position StartOfDocument(const Position&);
CORE_EXPORT PositionInFlatTree StartOfDocument(const PositionInFlatTree&);
CORE_EXPORT Position EndOfDocument(const Position&);
CORE_EXPORT PositionInFlatTree EndOfDocument(const PositionInFlatTree&);
CORE_EXPORT bool IsStartOfDocument(const Position&);
CORE_EXPORT bool IsEndOfDocument(const Position&);

// ----- editable content -----
CORE_EXPORT Position StartOfEditableContent(const Position&);
CORE_EXPORT PositionInFlatTree
StartOfEditableContent(const PositionInFlatTree&);
CORE_EXPORT Position EndOfEditableContent(const Position&);
CORE_EXPORT PositionInFlatTree EndOfEditableContent(const PositionInFlatTree&);
CORE_EXPORT bool IsEndOfEditableOrNonEditableContent(const Position&);
CORE_EXPORT bool IsEndOfEditableOrNonEditableContent(const PositionInFlatTree&);

// ----- paragraphs -----
// Definitions are in position_units_paragraph.cc.
CORE_EXPORT Position
StartOfParagraph(const Position&,
                 EditingBoundaryCrossingRule = kCannotCrossEditingBoundary);
CORE_EXPORT PositionInFlatTree
StartOfParagraph(const PositionInFlatTree&,
                 EditingBoundaryCrossingRule = kCannotCrossEditingBoundary);
CORE_EXPORT Position
EndOfParagraph(const Position&,
               EditingBoundaryCrossingRule = kCannotCrossEditingBoundary);
CORE_EXPORT PositionInFlatTree
EndOfParagraph(const PositionInFlatTree&,
               EditingBoundaryCrossingRule = kCannotCrossEditingBoundary);
CORE_EXPORT bool IsStartOfParagraph(
    const Position&,
    EditingBoundaryCrossingRule = kCannotCrossEditingBoundary);
CORE_EXPORT bool IsEndOfParagraph(
    const Position&,
    EditingBoundaryCrossingRule = kCannotCrossEditingBoundary);
CORE_EXPORT bool InSameParagraph(
    const Position&,
    const Position&,
    EditingBoundaryCrossingRule = kCannotCrossEditingBoundary);
CORE_EXPORT EphemeralRange ExpandToParagraphBoundary(const EphemeralRange&);

// Skips |pos| forward to the end of the editing boundary relative to
// |anchor|.  Used by NextPositionOf when kCanSkipOverEditingBoundary.
CORE_EXPORT Position SkipToEndOfEditingBoundary(const Position& pos,
                                                const Position& anchor);
CORE_EXPORT PositionInFlatTree
SkipToEndOfEditingBoundary(const PositionInFlatTree& pos,
                           const PositionInFlatTree& anchor);

// Skips |pos| backward to the start of the editing boundary relative to
// |anchor|.  Used by PreviousPositionOf when kCanSkipOverEditingBoundary.
CORE_EXPORT Position SkipToStartOfEditingBoundary(const Position& pos,
                                                  const Position& anchor);
CORE_EXPORT PositionInFlatTree
SkipToStartOfEditingBoundary(const PositionInFlatTree& pos,
                             const PositionInFlatTree& anchor);

// Returns the previous visually distinct candidate position, adjusted
// according to |rule|.  Unlike the VisiblePosition overload in
// visible_units.h, no CreateVisiblePosition canonicalization is performed.
CORE_EXPORT Position
PreviousPositionOf(const Position&,
                   EditingBoundaryCrossingRule = kCanCrossEditingBoundary);

// Returns the next visually distinct candidate position, adjusted
// according to |rule|.  Unlike the VisiblePosition overload in
// visible_units.h, no CreateVisiblePosition canonicalization is performed.
CORE_EXPORT Position
NextPositionOf(const Position&,
               EditingBoundaryCrossingRule = kCanCrossEditingBoundary);

CORE_EXPORT PositionInFlatTree
PreviousPositionOf(const PositionInFlatTree&,
                   EditingBoundaryCrossingRule = kCanCrossEditingBoundary);

CORE_EXPORT PositionInFlatTree
NextPositionOf(const PositionInFlatTree&,
               EditingBoundaryCrossingRule = kCanCrossEditingBoundary);

// Returns the Unicode code point immediately after |position|, using
// MostForwardCaretPosition to resolve to a text offset.  Returns 0 when no
// character follows.
CORE_EXPORT UChar32 CharacterAfter(const Position&);
CORE_EXPORT UChar32 CharacterAfter(const PositionInFlatTree&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_UNITS_H_
