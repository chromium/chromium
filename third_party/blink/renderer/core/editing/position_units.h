// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_UNITS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_UNITS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/editing_boundary.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_UNITS_H_
