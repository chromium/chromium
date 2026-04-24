// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_UNITS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_UNITS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"

namespace blink {

enum class SentenceTrailingSpaceBehavior { kIncludeSpace, kOmitSpace };

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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_UNITS_H_
