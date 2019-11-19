// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"

#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

namespace blink {

namespace {

// The calculation takes the following input:
// - An inline formatting context as a |LayoutBlockFlow|
// - An offset in the |text_content_| string of the above context
// - A TextAffinity
//
// The calculation iterates all inline fragments in the context, and tries to
// compute an NGCaretPosition using the "caret resolution process" below:
//
// The (offset, affinity) pair is compared against each inline fragment to see
// if the corresponding caret should be placed in the fragment, using the
// |TryResolveCaretPositionInXXX()| functions. These functions may return:
// - Failed, indicating that the caret must not be placed in the fragment;
// - Resolved, indicating that the care should be placed in the fragment, and
//   no further search is required. The result NGCaretPosition is returned
//   together.
// - FoundCandidate, indicating that the caret may be placed in the fragment;
//   however, further search may find a better position. The candidate
//   NGCaretPosition is also returned together.

enum class ResolutionType { kFailed, kFoundCandidate, kResolved };
struct CaretPositionResolution {
  STACK_ALLOCATED();

 public:
  ResolutionType type = ResolutionType::kFailed;
  NGCaretPosition caret_position;
};

bool CanResolveCaretPositionBeforeFragment(const NGInlineCursor& cursor,
                                           TextAffinity affinity) {
  if (affinity == TextAffinity::kDownstream)
    return true;
  if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled())
    return false;
  NGInlineCursor current_line(cursor);
  current_line.MoveToContainingLine();
  // A fragment after line wrap must be the first logical leaf in its line.
  NGInlineCursor first_logical_leaf(current_line);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  if (cursor != first_logical_leaf)
    return true;
  NGInlineCursor last_line(current_line);
  last_line.MoveToPreviousLine();
  return !last_line || !last_line.HasSoftWrapToNextLine();
}

bool CanResolveCaretPositionAfterFragment(const NGInlineCursor& cursor,
                                          TextAffinity affinity) {
  if (affinity == TextAffinity::kUpstream)
    return true;
  if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled())
    return false;
  NGInlineCursor current_line(cursor);
  current_line.MoveToContainingLine();
  // A fragment before line wrap must be the last logical leaf in its line.
  NGInlineCursor last_logical_leaf(current_line);
  last_logical_leaf.MoveToLastLogicalLeaf();
  if (cursor != last_logical_leaf)
    return true;
  return !current_line.HasSoftWrapToNextLine();
}

// Returns a |kFailed| resolution if |offset| doesn't belong to the text
// fragment. Otherwise, return either |kFoundCandidate| or |kResolved| depending
// on |affinity|.
CaretPositionResolution TryResolveCaretPositionInTextFragment(
    const NGInlineCursor& cursor,
    unsigned offset,
    TextAffinity affinity) {
  if (cursor.IsGeneratedText())
    return CaretPositionResolution();

  const NGOffsetMapping& mapping =
      *NGOffsetMapping::GetFor(cursor.CurrentLayoutObject());

  // A text fragment natually allows caret placement in offset range
  // [StartOffset(), EndOffset()], i.e., from before the first character to
  // after the last character.
  // Besides, leading/trailing bidi control characters are ignored since their
  // two sides are considered the same caret position. Hence, if there are n and
  // m leading and trailing bidi control characters, then the allowed offset
  // range is [StartOffset() - n, EndOffset() + m].
  // Note that we don't ignore other characters that are not in fragments. For
  // example, a trailing space of a line is not in any fragment, but its two
  // sides are still different caret positions, so we don't ignore it.
  const unsigned start_offset = cursor.CurrentTextStartOffset();
  const unsigned end_offset = cursor.CurrentTextEndOffset();
  if (offset < start_offset &&
      !mapping.HasBidiControlCharactersOnly(offset, start_offset))
    return CaretPositionResolution();
  if (offset > cursor.CurrentTextEndOffset() &&
      !mapping.HasBidiControlCharactersOnly(end_offset, offset))
    return CaretPositionResolution();

  offset = std::max(offset, start_offset);
  offset = std::min(offset, end_offset);
  NGCaretPosition candidate = {cursor, NGCaretPositionType::kAtTextOffset,
                               offset};

  // Offsets in the interior of a fragment can be resolved directly.
  if (offset > start_offset && offset < end_offset)
    return {ResolutionType::kResolved, candidate};

  if (offset == start_offset &&
      CanResolveCaretPositionBeforeFragment(cursor, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  if (offset == end_offset && !cursor.IsLineBreak() &&
      CanResolveCaretPositionAfterFragment(cursor, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  // We may have a better candidate
  return {ResolutionType::kFoundCandidate, candidate};
}

unsigned GetTextOffsetBefore(const Node& node) {
  // TODO(xiaochengh): Design more straightforward way to get text offset of
  // atomic inline box.
  DCHECK(node.GetLayoutObject()->IsAtomicInlineLevel());
  const Position before_node = Position::BeforeNode(node);
  base::Optional<unsigned> maybe_offset_before =
      NGOffsetMapping::GetFor(before_node)->GetTextContentOffset(before_node);
  // We should have offset mapping for atomic inline boxes.
  DCHECK(maybe_offset_before.has_value());
  return *maybe_offset_before;
}

// Returns a |kFailed| resolution if |offset| doesn't belong to the atomic
// inline box fragment. Otherwise, return either |kFoundCandidate| or
// |kResolved| depending on |affinity|.
CaretPositionResolution TryResolveCaretPositionByBoxFragmentSide(
    const NGInlineCursor& cursor,
    unsigned offset,
    TextAffinity affinity) {
  const Node* const node = cursor.CurrentNode();
  // There is no caret position at a pseudo or generated box side.
  if (!node || node->IsPseudoElement()) {
    // TODO(xiaochengh): This leads to false negatives for, e.g., RUBY, where an
    // anonymous wrapping inline block is created.
    return CaretPositionResolution();
  }

  const unsigned offset_before = GetTextOffsetBefore(*node);
  const unsigned offset_after = offset_before + 1;
  // TODO(xiaochengh): Ignore bidi control characters before & after the box.
  if (offset != offset_before && offset != offset_after)
    return CaretPositionResolution();
  const NGCaretPositionType position_type =
      offset == offset_before ? NGCaretPositionType::kBeforeBox
                              : NGCaretPositionType::kAfterBox;
  NGCaretPosition candidate{cursor, position_type, base::nullopt};

  if (offset == offset_before &&
      CanResolveCaretPositionBeforeFragment(cursor, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  if (offset == offset_after &&
      CanResolveCaretPositionAfterFragment(cursor, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  return {ResolutionType::kFoundCandidate, candidate};
}

CaretPositionResolution TryResolveCaretPositionWithFragment(
    const NGInlineCursor& cursor,
    unsigned offset,
    TextAffinity affinity) {
  if (cursor.IsText())
    return TryResolveCaretPositionInTextFragment(cursor, offset, affinity);
  if (cursor.IsAtomicInline())
    return TryResolveCaretPositionByBoxFragmentSide(cursor, offset, affinity);
  return CaretPositionResolution();
}

bool NeedsBidiAdjustment(const NGCaretPosition& caret_position) {
  if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled())
    return false;
  if (caret_position.IsNull())
    return false;
  if (caret_position.position_type != NGCaretPositionType::kAtTextOffset)
    return true;
  DCHECK(caret_position.text_offset.has_value());
  const unsigned start_offset = caret_position.cursor.CurrentTextStartOffset();
  const unsigned end_offset = caret_position.cursor.CurrentTextEndOffset();
  DCHECK_GE(*caret_position.text_offset, start_offset);
  DCHECK_LE(*caret_position.text_offset, end_offset);
  // Bidi adjustment is needed only for caret positions at bidi boundaries.
  // Caret positions in the middle of a text fragment can't be at bidi
  // boundaries, and hence, don't need any adjustment.
  return *caret_position.text_offset == start_offset ||
         *caret_position.text_offset == end_offset;
}

NGCaretPosition AdjustCaretPositionForBidiText(
    const NGCaretPosition& caret_position) {
  if (!NeedsBidiAdjustment(caret_position))
    return caret_position;
  return BidiAdjustment::AdjustForCaretPositionResolution(caret_position);
}

bool IsUpstreamAfterLineBreak(const NGCaretPosition& caret_position) {
  if (caret_position.position_type != NGCaretPositionType::kAtTextOffset)
    return false;

  DCHECK(caret_position.cursor.IsNotNull());
  DCHECK(caret_position.text_offset.has_value());

  if (!caret_position.cursor.IsLineBreak())
    return false;
  return *caret_position.text_offset ==
         caret_position.cursor.CurrentTextEndOffset();
}

NGCaretPosition BetterCandidateBetween(const NGCaretPosition& current,
                                       const NGCaretPosition& other,
                                       unsigned offset,
                                       TextAffinity affinity) {
  DCHECK(!other.IsNull());
  if (current.IsNull())
    return other;

  // There shouldn't be too many cases where we have multiple candidates.
  // Make sure all of them are captured and handled here.

  // Only known case: either |current| or |other| is upstream after line break.
  DCHECK_EQ(affinity, TextAffinity::kUpstream);
  if (IsUpstreamAfterLineBreak(current)) {
    DCHECK(!IsUpstreamAfterLineBreak(other));
    return other;
  }
  DCHECK(IsUpstreamAfterLineBreak(other));
  return current;
}

}  // namespace

// The main function for compute an NGCaretPosition. See the comments at the top
// of this file for details.
NGCaretPosition ComputeNGCaretPosition(const LayoutBlockFlow& context,
                                       unsigned offset,
                                       TextAffinity affinity) {
  NGInlineCursor cursor(context);

  NGCaretPosition candidate;
  for (; cursor; cursor.MoveToNext()) {
    const CaretPositionResolution resolution =
        TryResolveCaretPositionWithFragment(cursor, offset, affinity);

    if (resolution.type == ResolutionType::kFailed)
      continue;

    // TODO(xiaochengh): Handle caret poisition in empty container (e.g. empty
    // line box).

    if (resolution.type == ResolutionType::kResolved)
      return AdjustCaretPositionForBidiText(resolution.caret_position);

    DCHECK_EQ(ResolutionType::kFoundCandidate, resolution.type);
    candidate = BetterCandidateBetween(candidate, resolution.caret_position,
                                       offset, affinity);
  }

  return AdjustCaretPositionForBidiText(candidate);
}

NGCaretPosition ComputeNGCaretPosition(const PositionWithAffinity& position) {
  LayoutBlockFlow* context =
      NGInlineFormattingContextOf(position.GetPosition());
  if (!context)
    return NGCaretPosition();

  const NGOffsetMapping* mapping = NGInlineNode::GetOffsetMapping(context);
  DCHECK(mapping);
  const base::Optional<unsigned> maybe_offset =
      mapping->GetTextContentOffset(position.GetPosition());
  if (!maybe_offset.has_value()) {
    // TODO(xiaochengh): Investigate if we reach here.
    NOTREACHED();
    return NGCaretPosition();
  }

  const unsigned offset = *maybe_offset;
  const TextAffinity affinity = position.Affinity();
  return ComputeNGCaretPosition(*context, offset, affinity);
}

Position NGCaretPosition::ToPositionInDOMTree() const {
  return ToPositionInDOMTreeWithAffinity().GetPosition();
}

PositionWithAffinity NGCaretPosition::ToPositionInDOMTreeWithAffinity() const {
  if (IsNull())
    return PositionWithAffinity();
  switch (position_type) {
    case NGCaretPositionType::kBeforeBox:
      if (cursor.CurrentNode())
        return PositionWithAffinity();
      return PositionWithAffinity(Position::BeforeNode(*cursor.CurrentNode()),
                                  TextAffinity::kDownstream);
    case NGCaretPositionType::kAfterBox:
      if (cursor.CurrentNode())
        return PositionWithAffinity();
      return PositionWithAffinity(Position::AfterNode(*cursor.CurrentNode()),
                                  TextAffinity::kUpstreamIfPossible);
    case NGCaretPositionType::kAtTextOffset:
      // In case of ::first-letter, |cursor.CurrentNode()| is null.
      DCHECK(text_offset.has_value());
      const NGOffsetMapping* mapping =
          NGOffsetMapping::GetFor(cursor.CurrentLayoutObject());
      const TextAffinity affinity =
          *text_offset == cursor.CurrentTextEndOffset()
              ? TextAffinity::kUpstreamIfPossible
              : TextAffinity::kDownstream;
      const Position position = affinity == TextAffinity::kDownstream
                                    ? mapping->GetLastPosition(*text_offset)
                                    : mapping->GetFirstPosition(*text_offset);
      if (position.IsNull())
        return PositionWithAffinity();
      return PositionWithAffinity(position, affinity);
  }
  NOTREACHED();
  return PositionWithAffinity();
}

}  // namespace blink
