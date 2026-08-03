// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_VISUAL_CARET_MOVEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_VISUAL_CARET_MOVEMENT_H_

#include <unicode/ubidi.h>

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class InlineCursor;

// Result of a visual caret movement operation. Contains the new caret position
// and the bidi embedding level of the fragment the caret landed on. The bidi
// level is used to disambiguate caret position at bidi boundaries on the next
// keystroke (analogous to Firefox's mCaretBidiLevel).
//
// The position is stored as a raw PositionInFlatTreeWithAffinity (not a
// VisiblePositionInFlatTree) to avoid the canonicalization performed by
// CreateVisiblePosition → CanonicalPositionOf → MostBackwardCaretPosition.
// At bidi boundaries, canonicalization shifts the position to a different DOM
// location, destroying the fragment-level precision that the visual movement
// algorithm computes. By keeping the raw position, we preserve the exact
// text-content-offset that FindFragmentForPosition can resolve correctly on
// the next keystroke.
struct CORE_EXPORT VisualCaretMoveResult {
  STACK_ALLOCATED();

 public:
  PositionInFlatTreeWithAffinity position;
  std::optional<UBiDiLevel> bidi_level;
  // True if the caret landed at a bidi boundary entry point (same visual x
  // as the previous position). The next boundary crossing from this position
  // should skip the shared-x entry point to produce visible movement.
  bool entered_bidi_run = false;
};

// Computes the next caret position visually to the right of |position|.
//
// |input_bidi_level| is the bidi embedding level from the previous movement,
// used to disambiguate which fragment the caret belongs to at bidi boundaries.
// Pass std::nullopt for the first movement or after a mouse click.
//
// |entered_bidi_run| indicates whether the previous movement placed the caret
// at a bidi boundary entry point. When true, the next boundary crossing is
// treated as an EXIT (skip the shared-x entry point to produce visible
// movement). When false, boundary crossings are treated as ENTRY (stop at the
// entry point).
//
// Returns a VisualCaretMoveResult containing the new position and the bidi
// level of the fragment the caret landed on. The position is null if no
// movement is possible (caller should fall back to logical movement).
CORE_EXPORT VisualCaretMoveResult
MoveCaretVisuallyRight(const PositionInFlatTreeWithAffinity& position,
                       std::optional<UBiDiLevel> input_bidi_level,
                       bool entered_bidi_run = false);

// Mirror of MoveCaretVisuallyRight -- moves visually to the left.
CORE_EXPORT VisualCaretMoveResult
MoveCaretVisuallyLeft(const PositionInFlatTreeWithAffinity& position,
                      std::optional<UBiDiLevel> input_bidi_level,
                      bool entered_bidi_run = false);

// Exposed in the header and CORE_EXPORTed for unit testing
// (visual_caret_movement_test.cc verifies fragment-level offset logic).
namespace internal {

// Returns the offset at the visual start of a text fragment.
// For LTR fragments: TextStartOffset()
// For RTL fragments: TextEndOffset()
CORE_EXPORT wtf_size_t VisualStartOffset(const InlineCursor& cursor);

// Returns the offset at the visual end of a text fragment.
// For LTR fragments: TextEndOffset()
// For RTL fragments: TextStartOffset()
CORE_EXPORT wtf_size_t VisualEndOffset(const InlineCursor& cursor);

}  // namespace internal

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_VISUAL_CARET_MOVEMENT_H_
