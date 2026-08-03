// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visual_caret_movement.h"

#include <unicode/ubidi.h>

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/inline/inline_caret_position.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

namespace {

// Whether to position the caret before or after an atomic inline element.
enum class AtomicInlineEdge { kBefore, kAfter };

// ---------------------------------------------------------------------------
// VisualFragment: lightweight wrapper around InlineCursor for visual
// fragment traversal within a single line.
//
// Parallels AbstractInlineBox in bidi_adjustment.cc (which lives in an
// anonymous namespace and cannot be reused directly).
// ---------------------------------------------------------------------------
class VisualFragment {
  STACK_ALLOCATED();

 public:
  // Creates a null VisualFragment (IsNull() returns true).
  VisualFragment() = default;

  explicit VisualFragment(const InlineCursor& cursor) : is_null_(false) {
    InlineCursor line_box;
    line_box.MoveTo(cursor);
    line_box.MoveToContainingLine();
    line_cursor_ = line_box.CursorForDescendants();
    line_cursor_.MoveTo(cursor);
  }

  bool IsNull() const { return is_null_; }

  bool IsText() const {
    DCHECK(!IsNull());
    return line_cursor_.Current().IsText();
  }

  bool IsAtomicInline() const {
    DCHECK(!IsNull());
    return line_cursor_.Current().IsAtomicInline();
  }

  TextDirection Direction() const {
    DCHECK(!IsNull());
    if (IsText() || IsAtomicInline()) {
      return line_cursor_.Current().ResolvedDirection();
    }
    InlineCursor line_box;
    line_box.MoveTo(line_cursor_);
    line_box.MoveToContainingLine();
    return line_box.Current().BaseDirection();
  }

  UBiDiLevel BidiLevel() const {
    DCHECK(!IsNull());
    return line_cursor_.Current().BidiLevel();
  }

  wtf_size_t TextStartOffset() const {
    DCHECK(!IsNull());
    return line_cursor_.Current().TextStartOffset();
  }

  wtf_size_t TextEndOffset() const {
    DCHECK(!IsNull());
    return line_cursor_.Current().TextEndOffset();
  }

  const LayoutObject* GetLayoutObject() const {
    DCHECK(!IsNull());
    return line_cursor_.Current().GetLayoutObject();
  }

  InlineCursor GetCursor() const {
    DCHECK(!IsNull());
    return line_cursor_.CursorForMovingAcrossFragmentainer();
  }

  const InlineCursor& LineCursor() const {
    DCHECK(!IsNull());
    return line_cursor_;
  }

  VisualFragment NextLeafOnLine() const {
    DCHECK(!IsNull());
    InlineCursor cursor(line_cursor_);
    cursor.MoveToNextInlineLeafIgnoringLineBreak();
    if (!cursor) {
      return VisualFragment();
    }
    return VisualFragment(cursor);
  }

  VisualFragment PrevLeafOnLine() const {
    DCHECK(!IsNull());
    InlineCursor cursor(line_cursor_);
    cursor.MoveToPreviousInlineLeafIgnoringLineBreak();
    if (!cursor) {
      return VisualFragment();
    }
    return VisualFragment(cursor);
  }

 private:
  InlineCursor line_cursor_;
  bool is_null_ = true;
};

// ---------------------------------------------------------------------------
// Fragment resolution — bypasses ComputeInlineCaretPosition to avoid bidi
// adjustment that corrupts fragment identification at bidi boundaries.
// ---------------------------------------------------------------------------

// Given a DOM position, finds the containing fragment and text content offset
// directly using OffsetMapping + InlineCursor.
//
// At bidi boundaries, a single text content offset matches two fragments (e.g.,
// offset 3 in "abcאבג" matches both LTR[0,3) at its end and RTL[3,6) at its
// start). The |bidi_level| parameter disambiguates: if present, we prefer the
// fragment whose BidiLevel matches. If no bidi level is provided (nullopt), we
// fall back to first match.
//
// This function does NOT go through ComputeInlineCaretPosition, which applies
// BidiAdjustment that silently relocates positions at bidi boundaries —
// correct for logical movement but wrong for visual movement.
VisualFragment FindFragmentForPosition(
    const PositionInFlatTree& position,
    wtf_size_t* out_offset,
    std::optional<UBiDiLevel> bidi_level = std::nullopt) {
  if (position.IsNull()) {
    return VisualFragment();
  }

  Position dom_pos = ToPositionInDomTree(position);
  if (dom_pos.IsNull()) {
    return VisualFragment();
  }

  const Node* node = dom_pos.AnchorNode();
  if (!node) {
    return VisualFragment();
  }

  const LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object) {
    return VisualFragment();
  }

  // Non-text nodes (element boundaries): use ComputeInlineCaretPosition as
  // a fallback since the element itself won't have text fragments.
  // NOTE: This applies BidiAdjustment, which we normally bypass. This is
  // safe here because non-text nodes (e.g., <img>, <br>) don't have bidi
  // boundary ambiguity — there's only one fragment to resolve to.
  if (!node->IsTextNode()) {
    PositionWithAffinity pos_with_affinity(dom_pos, TextAffinity::kDownstream);
    InlineCaretPosition caret_pos =
        ComputeInlineCaretPosition(pos_with_affinity);
    if (!caret_pos) {
      return VisualFragment();
    }
    VisualFragment result(caret_pos.cursor);
    if (!result.IsNull() && result.IsText()) {
      *out_offset = caret_pos.text_offset.has_value()
                        ? *caret_pos.text_offset
                        : result.TextStartOffset();
    }
    return result;
  }

  // Get the text content offset for this DOM position.
  const OffsetMapping* mapping = OffsetMapping::GetFor(dom_pos);
  if (!mapping) {
    return VisualFragment();
  }

  std::optional<wtf_size_t> text_content_offset =
      mapping->GetTextContentOffset(dom_pos);
  if (!text_content_offset.has_value()) {
    return VisualFragment();
  }

  wtf_size_t tc_offset = *text_content_offset;

  // Scan all fragments for this layout object. At bidi boundaries, multiple
  // fragments match the same offset. We track the best match using bidi level
  // disambiguation.
  InlineCursor first_match;
  InlineCursor level_match;

  InlineCursor iter;
  iter.MoveTo(*layout_object);
  while (iter) {
    if (iter.Current().IsText()) {
      wtf_size_t start = iter.Current().TextStartOffset();
      wtf_size_t end = iter.Current().TextEndOffset();
      if (tc_offset >= start && tc_offset <= end) {
        // Interior of fragment — unambiguous.
        if (tc_offset > start && tc_offset < end) {
          *out_offset = tc_offset;
          return VisualFragment(iter);
        }

        // At a fragment boundary. Record first match as fallback.
        if (!first_match) {
          first_match = iter;
        }

        // If bidi level was provided, prefer the fragment whose level matches.
        if (bidi_level.has_value() && !level_match) {
          if (iter.Current().BidiLevel() == *bidi_level) {
            level_match = iter;
          }
        }
      }
    }
    iter.MoveToNextForSameLayoutObject();
  }

  // Priority: bidi level match > first match.
  InlineCursor best = level_match ? level_match : first_match;
  if (!best) {
    return VisualFragment();
  }

  *out_offset = tc_offset;
  return VisualFragment(best);
}

// ---------------------------------------------------------------------------
// Position conversion helpers
// ---------------------------------------------------------------------------

PositionInFlatTree TextContentOffsetToFlatTreePosition(
    const VisualFragment& box,
    wtf_size_t text_content_offset) {
  const LayoutObject* layout_object = box.GetLayoutObject();
  if (!layout_object) {
    return PositionInFlatTree();
  }

  const OffsetMapping* mapping = OffsetMapping::GetFor(layout_object);
  if (!mapping) {
    return PositionInFlatTree();
  }

  Position dom_position = mapping->GetFirstPosition(text_content_offset);
  if (dom_position.IsNull()) {
    return PositionInFlatTree();
  }

  return ToPositionInFlatTree(dom_position);
}

PositionInFlatTreeWithAffinity MakePositionWithAffinity(
    const VisualFragment& box,
    wtf_size_t text_content_offset,
    TextAffinity affinity) {
  PositionInFlatTree pos =
      TextContentOffsetToFlatTreePosition(box, text_content_offset);
  if (pos.IsNull()) {
    return PositionInFlatTreeWithAffinity();
  }
  return PositionInFlatTreeWithAffinity(pos, affinity);
}

PositionInFlatTreeWithAffinity MakeAtomicInlinePosition(
    const VisualFragment& box,
    AtomicInlineEdge edge,
    TextAffinity affinity) {
  const LayoutObject* layout_object = box.GetLayoutObject();
  if (!layout_object || !layout_object->GetNode()) {
    return PositionInFlatTreeWithAffinity();
  }

  const Node* node = layout_object->GetNode();
  Position dom_pos = edge == AtomicInlineEdge::kBefore
                         ? Position::BeforeNode(*node)
                         : Position::AfterNode(*node);
  PositionInFlatTree flat_pos = ToPositionInFlatTree(dom_pos);
  return PositionInFlatTreeWithAffinity(flat_pos, affinity);
}

// ---------------------------------------------------------------------------
// Grapheme-cluster-aware offset advancement
// ---------------------------------------------------------------------------

wtf_size_t NextGraphemeOffset(const VisualFragment& box, wtf_size_t offset) {
  const LayoutObject* layout_object = box.GetLayoutObject();
  if (!layout_object || !layout_object->GetNode()) {
    return offset;
  }
  const Node& node = *layout_object->GetNode();
  const OffsetMapping* mapping = OffsetMapping::GetFor(layout_object);
  if (!mapping) {
    return offset;
  }

  const OffsetMappingUnit* unit = nullptr;
  auto units = mapping->GetMappingUnitsForLayoutObject(*layout_object);
  for (const auto& u : units) {
    if (u.TextContentStart() <= offset && offset <= u.TextContentEnd()) {
      unit = &u;
      break;
    }
  }
  if (!unit) {
    return offset;
  }

  wtf_size_t dom_offset = unit->ConvertTextContentToFirstDOMOffset(offset);
  wtf_size_t next_dom = NextGraphemeBoundaryOf(node, dom_offset);
  if (next_dom <= dom_offset) {
    return offset;
  }

  return unit->ConvertDOMOffsetToTextContent(next_dom);
}

wtf_size_t PreviousGraphemeOffset(const VisualFragment& box,
                                  wtf_size_t offset) {
  const LayoutObject* layout_object = box.GetLayoutObject();
  if (!layout_object || !layout_object->GetNode()) {
    return offset;
  }
  const Node& node = *layout_object->GetNode();
  const OffsetMapping* mapping = OffsetMapping::GetFor(layout_object);
  if (!mapping) {
    return offset;
  }

  const OffsetMappingUnit* unit = nullptr;
  auto units = mapping->GetMappingUnitsForLayoutObject(*layout_object);
  for (const auto& u : units) {
    if (u.TextContentStart() <= offset && offset <= u.TextContentEnd()) {
      unit = &u;
      break;
    }
  }
  if (!unit) {
    return offset;
  }

  wtf_size_t dom_offset = unit->ConvertTextContentToFirstDOMOffset(offset);
  wtf_size_t prev_dom = PreviousGraphemeBoundaryOf(node, dom_offset);
  if (prev_dom >= dom_offset) {
    return offset;
  }

  return unit->ConvertDOMOffsetToTextContent(prev_dom);
}

// ---------------------------------------------------------------------------
// Cross-Line Movement helpers
// ---------------------------------------------------------------------------

VisualCaretMoveResult MoveToStartOfNextVisualLine(
    const InlineCursor& current_cursor) {
  InlineCursor line_box;
  line_box.MoveTo(current_cursor);
  line_box.MoveToContainingLine();
  line_box.MoveToNextLine();
  if (!line_box) {
    return {};
  }

  InlineCursor descendants = line_box.CursorForDescendants();
  descendants.MoveToNextInlineLeafIgnoringLineBreak();
  if (!descendants) {
    return {};
  }

  VisualFragment first_on_line(descendants);
  if (first_on_line.IsText()) {
    wtf_size_t start_offset =
        internal::VisualStartOffset(first_on_line.GetCursor());
    PositionInFlatTreeWithAffinity pos = MakePositionWithAffinity(
        first_on_line, start_offset, TextAffinity::kDownstream);
    if (pos.IsNotNull()) {
      return {pos, first_on_line.BidiLevel()};
    }
  } else if (first_on_line.IsAtomicInline()) {
    PositionInFlatTreeWithAffinity pos = MakeAtomicInlinePosition(
        first_on_line, AtomicInlineEdge::kBefore, TextAffinity::kDownstream);
    if (pos.IsNotNull()) {
      return {pos, first_on_line.BidiLevel()};
    }
  }

  return {};
}

VisualCaretMoveResult MoveToEndOfPreviousVisualLine(
    const InlineCursor& current_cursor) {
  InlineCursor line_box;
  line_box.MoveTo(current_cursor);
  line_box.MoveToContainingLine();
  line_box.MoveToPreviousLine();
  if (!line_box) {
    return {};
  }

  InlineCursor descendants = line_box.CursorForDescendants();
  VisualFragment last_on_line;
  descendants.MoveToNextInlineLeafIgnoringLineBreak();
  while (descendants) {
    last_on_line = VisualFragment(descendants);
    descendants.MoveToNextInlineLeafIgnoringLineBreak();
  }

  if (last_on_line.IsNull()) {
    return {};
  }

  if (last_on_line.IsText()) {
    wtf_size_t end_offset = internal::VisualEndOffset(last_on_line.GetCursor());
    PositionInFlatTreeWithAffinity pos = MakePositionWithAffinity(
        last_on_line, end_offset, TextAffinity::kUpstream);
    if (pos.IsNotNull()) {
      return {pos, last_on_line.BidiLevel()};
    }
  } else if (last_on_line.IsAtomicInline()) {
    PositionInFlatTreeWithAffinity pos = MakeAtomicInlinePosition(
        last_on_line, AtomicInlineEdge::kAfter, TextAffinity::kUpstream);
    if (pos.IsNotNull()) {
      return {pos, last_on_line.BidiLevel()};
    }
  }

  return {};
}

// ---------------------------------------------------------------------------
// Start state resolution
// ---------------------------------------------------------------------------

struct VisualMovementState {
  STACK_ALLOCATED();

 public:
  VisualFragment box;
  wtf_size_t offset = 0;
  bool is_after_atomic = false;
  bool is_before_atomic = false;
};

VisualMovementState ResolveStartState(
    const PositionInFlatTreeWithAffinity& position_with_affinity,
    std::optional<UBiDiLevel> bidi_level) {
  VisualMovementState state;
  const PositionInFlatTree& position = position_with_affinity.GetPosition();

  // Try direct fragment resolution with bidi level disambiguation.
  wtf_size_t tc_offset = 0;
  state.box = FindFragmentForPosition(position, &tc_offset, bidi_level);

  if (!state.box.IsNull() && state.box.IsText()) {
    state.offset = tc_offset;
    return state;
  }

  // Fallback: use ComputeInlineCaretPosition for atomic inlines and
  // other non-text cases.
  Position dom_pos = ToPositionInDomTree(position);
  if (dom_pos.IsNull()) {
    return state;
  }

  PositionWithAffinity pos_with_affinity(dom_pos,
                                         position_with_affinity.Affinity());
  InlineCaretPosition caret_pos = ComputeInlineCaretPosition(pos_with_affinity);
  if (!caret_pos) {
    return state;
  }

  state.box = VisualFragment(caret_pos.cursor);
  if (state.box.IsNull()) {
    return state;
  }

  if (caret_pos.position_type == InlineCaretPositionType::kAtTextOffset &&
      caret_pos.text_offset.has_value()) {
    state.offset = *caret_pos.text_offset;
  } else if (caret_pos.position_type == InlineCaretPositionType::kAfterBox) {
    state.is_after_atomic = true;
    if (state.box.IsText()) {
      state.offset = state.box.TextEndOffset();
    }
  } else if (caret_pos.position_type == InlineCaretPositionType::kBeforeBox) {
    state.is_before_atomic = true;
    if (state.box.IsText()) {
      state.offset = state.box.TextStartOffset();
    }
  }

  return state;
}

// ---------------------------------------------------------------------------
// Helper: create a VisualCaretMoveResult for a text fragment landing.
// ---------------------------------------------------------------------------

VisualCaretMoveResult MakeTextResult(const VisualFragment& box,
                                     wtf_size_t offset,
                                     TextAffinity affinity) {
  PositionInFlatTreeWithAffinity pos =
      MakePositionWithAffinity(box, offset, affinity);
  if (pos.IsNotNull()) {
    return {pos, box.BidiLevel()};
  }
  return {};
}

VisualCaretMoveResult MakeAtomicResult(const VisualFragment& box,
                                       AtomicInlineEdge edge,
                                       TextAffinity affinity) {
  PositionInFlatTreeWithAffinity pos =
      MakeAtomicInlinePosition(box, edge, affinity);
  if (pos.IsNotNull()) {
    return {pos, box.BidiLevel()};
  }
  return {};
}

}  // namespace

// ---------------------------------------------------------------------------
// internal:: helpers (exposed for testing)
// ---------------------------------------------------------------------------

namespace internal {

wtf_size_t VisualStartOffset(const InlineCursor& cursor) {
  if (IsLtr(cursor.Current().ResolvedDirection())) {
    return cursor.Current().TextStartOffset();
  }
  return cursor.Current().TextEndOffset();
}

wtf_size_t VisualEndOffset(const InlineCursor& cursor) {
  if (IsLtr(cursor.Current().ResolvedDirection())) {
    return cursor.Current().TextEndOffset();
  }
  return cursor.Current().TextStartOffset();
}

}  // namespace internal

// ---------------------------------------------------------------------------
// MoveCaretVisuallyRight
//
// IMPORTANT: This function is a mirror of MoveCaretVisuallyLeft. Any
// algorithmic changes here MUST be reflected there and vice versa.
//
// Three-step algorithm:
//   Step 1: Resolve which inline fragment contains |position| using the
//           input bidi level to disambiguate at boundaries.
//   Step 2: Try to advance within the current fragment (LTR: increment,
//           RTL: decrement).
//   Step 3: At fragment edge — cross to the next visual fragment on the
//           line, or cross to the next line.
// ---------------------------------------------------------------------------

VisualCaretMoveResult MoveCaretVisuallyRight(
    const PositionInFlatTreeWithAffinity& position_with_affinity,
    std::optional<UBiDiLevel> input_bidi_level,
    bool entered_bidi_run) {
  const PositionInFlatTree& position = position_with_affinity.GetPosition();
  VisualMovementState state =
      ResolveStartState(position_with_affinity, input_bidi_level);
  if (state.box.IsNull()) {
    return {};
  }

  VisualFragment box = state.box;
  wtf_size_t offset = state.offset;

  // Handle "after atomic inline" — move to next visual fragment.
  if (state.is_after_atomic && box.IsAtomicInline()) {
    VisualFragment next = box.NextLeafOnLine();
    if (next.IsNull()) {
      return MoveToStartOfNextVisualLine(box.LineCursor());
    }
    if (next.IsText()) {
      wtf_size_t entry = internal::VisualStartOffset(next.GetCursor());
      return MakeTextResult(next, entry, TextAffinity::kDownstream);
    }
    if (next.IsAtomicInline()) {
      return MakeAtomicResult(next, AtomicInlineEdge::kBefore,
                              TextAffinity::kDownstream);
    }
    return {};
  }

  // Step 2: Try to advance within the current fragment.
  // When advancing within a fragment, preserve the entered_bidi_run flag
  // from the input. This flag indicates "I entered this bidi run via a
  // boundary crossing" and must survive through all within-fragment steps
  // so that when we eventually reach the other edge of the fragment,
  // Step 3 correctly classifies the next crossing as an EXIT.
  if (box.IsText()) {
    if (IsLtr(box.Direction())) {
      if (offset < box.TextEndOffset()) {
        wtf_size_t new_offset = NextGraphemeOffset(box, offset);
        if (new_offset > offset && new_offset <= box.TextEndOffset()) {
          VisualCaretMoveResult result =
              MakeTextResult(box, new_offset, TextAffinity::kDownstream);
          result.entered_bidi_run = entered_bidi_run;
          return result;
        }
      }
    } else {
      if (offset > box.TextStartOffset()) {
        wtf_size_t new_offset = PreviousGraphemeOffset(box, offset);
        if (new_offset < offset && new_offset >= box.TextStartOffset()) {
          VisualCaretMoveResult result =
              MakeTextResult(box, new_offset, TextAffinity::kUpstream);
          result.entered_bidi_run = entered_bidi_run;
          return result;
        }
      }
    }
  }

  // Step 3: At the visual end of the current fragment. Cross to next.
  //
  // At bidi boundaries, the entry point of the next fragment is at the same
  // visual x-position as the exit point of the current fragment. There are
  // two sub-cases:
  //
  // (a) ENTRY: We finished traversing the current fragment and are entering
  //     a new bidi run for the first time. The visual entry point of the
  //     next fragment is the correct destination. We set
  //     entered_bidi_run=true so that the NEXT crossing (after traversing
  //     the opposite-direction run) is recognized as an EXIT.
  //
  // (b) EXIT: We previously entered the opposite-direction run (indicated by
  //     entered_bidi_run=true from the previous keystroke), traversed it
  //     completely, and are now exiting back. The visual entry point of the
  //     next fragment coincides with our current visual x, so we advance
  //     one step past it to produce visible movement.
  //
  // Detection: entered_bidi_run alone determines EXIT. The old heuristic
  // (input_bidi_level == box.BidiLevel()) was a false positive: it also
  // triggered for normal traversal reaching the end of a fragment (e.g.,
  // traversing LTR "Hello" then crossing into RTL text — input_bidi_level
  // is 0, box is level 0, so the heuristic wrongly said EXIT). Only the
  // explicit entered_bidi_run flag from the previous keystroke can
  // distinguish "I entered this run via a bidi crossing" from "I arrived
  // here normally."
  VisualFragment next = box.NextLeafOnLine();
  if (!next.IsNull()) {
    if (next.IsText()) {
      wtf_size_t entry = internal::VisualStartOffset(next.GetCursor());
      TextAffinity aff = IsLtr(next.Direction()) ? TextAffinity::kDownstream
                                                 : TextAffinity::kUpstream;

      bool is_bidi_boundary =
          box.IsText() && box.BidiLevel() != next.BidiLevel();

      if (is_bidi_boundary) {
        // Both ENTRY and EXIT at bidi boundaries advance one step past the
        // shared boundary point to produce visible movement. The boundary
        // point (VisualStartOffset) sits at the same visual x-coordinate as
        // the exit point of the current fragment, so stopping there would
        // look like the caret didn't move.
        //
        // ENTRY (entered_bidi_run=false): First time crossing into a new
        //   bidi run. Advance one step and set entered_bidi_run=true so
        //   that when we eventually reach the other edge, the crossing is
        //   recognized as an EXIT.
        //
        // EXIT (entered_bidi_run=true): Returning from a bidi run we
        //   previously entered. Advance one step and clear the flag.
        wtf_size_t advanced = entry;
        if (IsLtr(next.Direction())) {
          advanced = NextGraphemeOffset(next, entry);
        } else {
          advanced = PreviousGraphemeOffset(next, entry);
        }
        if (advanced != entry) {
          VisualCaretMoveResult result = MakeTextResult(next, advanced, aff);
          // ENTRY: set flag so future exit is detected.
          // EXIT: clear flag.
          result.entered_bidi_run = !entered_bidi_run;
          return result;
        }
        // Single-char or edge fragment: try the fragment after that.
        VisualFragment after_next = next.NextLeafOnLine();
        if (!after_next.IsNull() && after_next.IsText()) {
          wtf_size_t after_entry =
              internal::VisualStartOffset(after_next.GetCursor());
          TextAffinity after_aff = IsLtr(after_next.Direction())
                                       ? TextAffinity::kDownstream
                                       : TextAffinity::kUpstream;
          VisualCaretMoveResult result =
              MakeTextResult(after_next, after_entry, after_aff);
          result.entered_bidi_run = false;
          return result;
        }
      } else {
        // Same-direction crossing: not a bidi boundary, just enter normally.
        // With raw positions (no VisiblePosition canonicalization), span
        // boundaries are handled correctly because the position is preserved
        // exactly as computed. Check if the raw position is the same as
        // the input position (which can happen when the entry offset is
        // at the exact boundary between fragments in the same DOM node).
        VisualCaretMoveResult result = MakeTextResult(next, entry, aff);
        if (result.position.IsNotNull() &&
            result.position.GetPosition() == position) {
          // Entry point is the same as current position. Advance
          // one grapheme into the next fragment.
          wtf_size_t advanced = entry;
          if (IsLtr(next.Direction())) {
            advanced = NextGraphemeOffset(next, entry);
          } else {
            advanced = PreviousGraphemeOffset(next, entry);
          }
          if (advanced != entry) {
            result = MakeTextResult(next, advanced, aff);
          }
        }
        result.entered_bidi_run = false;
        return result;
      }
    }
    if (next.IsAtomicInline()) {
      return MakeAtomicResult(next, AtomicInlineEdge::kBefore,
                              TextAffinity::kDownstream);
    }
  }

  // End of line — try Cross-Line Movement.
  VisualCaretMoveResult cross_line =
      MoveToStartOfNextVisualLine(box.LineCursor());
  if (cross_line.position.IsNotNull()) {
    return cross_line;
  }

  // End of document — return current position unchanged.
  return {PositionInFlatTreeWithAffinity(position, TextAffinity::kDownstream),
          input_bidi_level};
}

// ---------------------------------------------------------------------------
// MoveCaretVisuallyLeft — mirror of MoveCaretVisuallyRight.
//
// IMPORTANT: This function is a mirror of MoveCaretVisuallyRight. Any
// algorithmic changes here MUST be reflected there and vice versa.
// ---------------------------------------------------------------------------

VisualCaretMoveResult MoveCaretVisuallyLeft(
    const PositionInFlatTreeWithAffinity& position_with_affinity,
    std::optional<UBiDiLevel> input_bidi_level,
    bool entered_bidi_run) {
  const PositionInFlatTree& position = position_with_affinity.GetPosition();
  VisualMovementState state =
      ResolveStartState(position_with_affinity, input_bidi_level);
  if (state.box.IsNull()) {
    return {};
  }

  VisualFragment box = state.box;
  wtf_size_t offset = state.offset;

  // Handle "before atomic inline" — move to previous visual fragment.
  if (state.is_before_atomic && box.IsAtomicInline()) {
    VisualFragment prev = box.PrevLeafOnLine();
    if (prev.IsNull()) {
      return MoveToEndOfPreviousVisualLine(box.LineCursor());
    }
    if (prev.IsText()) {
      wtf_size_t entry = internal::VisualEndOffset(prev.GetCursor());
      return MakeTextResult(prev, entry, TextAffinity::kUpstream);
    }
    if (prev.IsAtomicInline()) {
      return MakeAtomicResult(prev, AtomicInlineEdge::kAfter,
                              TextAffinity::kUpstream);
    }
    return {};
  }

  // Step 2: Try to retreat within the current fragment.
  // Preserve entered_bidi_run through within-fragment movement (mirror of
  // MoveRight Step 2).
  if (box.IsText()) {
    if (IsLtr(box.Direction())) {
      if (offset > box.TextStartOffset()) {
        wtf_size_t new_offset = PreviousGraphemeOffset(box, offset);
        if (new_offset < offset && new_offset >= box.TextStartOffset()) {
          VisualCaretMoveResult result =
              MakeTextResult(box, new_offset, TextAffinity::kUpstream);
          result.entered_bidi_run = entered_bidi_run;
          return result;
        }
      }
    } else {
      if (offset < box.TextEndOffset()) {
        wtf_size_t new_offset = NextGraphemeOffset(box, offset);
        if (new_offset > offset && new_offset <= box.TextEndOffset()) {
          VisualCaretMoveResult result =
              MakeTextResult(box, new_offset, TextAffinity::kDownstream);
          result.entered_bidi_run = entered_bidi_run;
          return result;
        }
      }
    }
  }

  // Step 3: At the visual start of the current fragment. Cross to previous.
  //
  // Mirror of MoveRight Step 3. Uses entered_bidi_run exclusively for
  // EXIT detection. The old heuristic (input_bidi_level == box.BidiLevel())
  // was removed because it caused false-positive EXIT detection.
  VisualFragment prev = box.PrevLeafOnLine();
  if (!prev.IsNull()) {
    if (prev.IsText()) {
      wtf_size_t entry = internal::VisualEndOffset(prev.GetCursor());
      TextAffinity aff = IsLtr(prev.Direction()) ? TextAffinity::kUpstream
                                                 : TextAffinity::kDownstream;

      bool is_bidi_boundary =
          box.IsText() && box.BidiLevel() != prev.BidiLevel();

      if (is_bidi_boundary) {
        // Mirror of MoveRight: both ENTRY and EXIT advance one step past
        // the shared boundary point to produce visible movement.
        wtf_size_t retreated = entry;
        if (IsLtr(prev.Direction())) {
          retreated = PreviousGraphemeOffset(prev, entry);
        } else {
          retreated = NextGraphemeOffset(prev, entry);
        }
        if (retreated != entry) {
          VisualCaretMoveResult result = MakeTextResult(prev, retreated, aff);
          result.entered_bidi_run = !entered_bidi_run;
          return result;
        }
        VisualFragment before_prev = prev.PrevLeafOnLine();
        if (!before_prev.IsNull() && before_prev.IsText()) {
          wtf_size_t before_entry =
              internal::VisualEndOffset(before_prev.GetCursor());
          TextAffinity before_aff = IsLtr(before_prev.Direction())
                                        ? TextAffinity::kUpstream
                                        : TextAffinity::kDownstream;
          VisualCaretMoveResult result =
              MakeTextResult(before_prev, before_entry, before_aff);
          result.entered_bidi_run = false;
          return result;
        }
      } else {
        // Same-direction crossing: not a bidi boundary, just enter normally.
        // Mirror of MoveRight: with raw positions, compare directly to detect
        // same-position entries at fragment boundaries.
        VisualCaretMoveResult result = MakeTextResult(prev, entry, aff);
        if (result.position.IsNotNull() &&
            result.position.GetPosition() == position) {
          wtf_size_t retreated = entry;
          if (IsLtr(prev.Direction())) {
            retreated = PreviousGraphemeOffset(prev, entry);
          } else {
            retreated = NextGraphemeOffset(prev, entry);
          }
          if (retreated != entry) {
            result = MakeTextResult(prev, retreated, aff);
          }
        }
        result.entered_bidi_run = false;
        return result;
      }
    }
    if (prev.IsAtomicInline()) {
      return MakeAtomicResult(prev, AtomicInlineEdge::kAfter,
                              TextAffinity::kUpstream);
    }
  }

  // Beginning of line — try Cross-Line Movement.
  VisualCaretMoveResult cross_line =
      MoveToEndOfPreviousVisualLine(box.LineCursor());
  if (cross_line.position.IsNotNull()) {
    return cross_line;
  }

  // Start of document.
  return {PositionInFlatTreeWithAffinity(position, TextAffinity::kUpstream),
          input_bidi_level};
}

}  // namespace blink
