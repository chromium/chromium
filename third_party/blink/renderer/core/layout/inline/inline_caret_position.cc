// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_caret_position.h"

#include "third_party/blink/renderer/core/editing/bidi_adjustment.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

namespace {

// The calculation takes the following input:
// - An inline formatting context as a |LayoutBlockFlow|
// - An offset in the |text_content_| string of the above context
// - A TextAffinity
//
// The calculation iterates all inline fragments in the context, and tries to
// compute an InlineCaretPosition using the "caret resolution process" below:
//
// The (offset, affinity) pair is compared against each inline fragment to see
// if the corresponding caret should be placed in the fragment, using the
// |TryResolveInlineCaretPositionInXXX()| functions. These functions may return:
// - Failed, indicating that the caret must not be placed in the fragment;
// - Resolved, indicating that the care should be placed in the fragment, and
//   no further search is required. The result InlineCaretPosition is returned
//   together.
// - FoundCandidate, indicating that the caret may be placed in the fragment;
//   however, further search may find a better position. The candidate
//   InlineCaretPosition is also returned together.

enum class ResolutionType { kFailed, kFoundCandidate, kResolved };
struct InlineCaretPositionResolution {
  STACK_ALLOCATED();

 public:
  ResolutionType type = ResolutionType::kFailed;
  InlineCaretPosition caret_position;
};

bool CanResolveInlineCaretPositionBeforeFragment(const InlineCursor& cursor,
                                                 TextAffinity affinity) {
  if (affinity == TextAffinity::kDownstream) {
    return true;
  }
  if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled()) {
    return false;
  }
  InlineCursor current_line(cursor);
  current_line.MoveToContainingLine();
  // A fragment after line wrap must be the first logical leaf in its line.
  InlineCursor first_logical_leaf(current_line);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  if (cursor != first_logical_leaf) {
    return true;
  }
  InlineCursor last_line(current_line);
  last_line.MoveToPreviousLine();
  return !last_line || !last_line.Current().HasSoftWrapToNextLine();
}

bool CanResolveInlineCaretPositionAfterFragment(const InlineCursor& cursor,
                                                TextAffinity affinity) {
  if (affinity == TextAffinity::kUpstream) {
    return true;
  }
  if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled()) {
    return false;
  }
  InlineCursor current_line(cursor);
  current_line.MoveToContainingLine();
  // A fragment before line wrap must be the last logical leaf in its line.
  InlineCursor last_logical_leaf(current_line);
  last_logical_leaf.MoveToLastLogicalLeaf();
  if (cursor != last_logical_leaf) {
    return true;
  }
  return !current_line.Current().HasSoftWrapToNextLine();
}

// Returns a |kFailed| resolution if |offset| doesn't belong to the text
// fragment. Otherwise, return either |kFoundCandidate| or |kResolved| depending
// on |affinity|.
InlineCaretPositionResolution TryResolveInlineCaretPositionInTextFragment(
    const InlineCursor& cursor,
    unsigned offset,
    TextAffinity affinity) {
  if (cursor.Current().IsGeneratedText()) {
    return InlineCaretPositionResolution();
  }

  const OffsetMapping& mapping =
      *OffsetMapping::GetFor(cursor.Current().GetLayoutObject());

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
  const TextOffsetRange current_offset = cursor.Current().TextOffset();
  const unsigned start_offset = current_offset.start;
  const unsigned end_offset = current_offset.end;
  if (offset < start_offset &&
      !mapping.HasBidiControlCharactersOnly(offset, start_offset)) {
    return InlineCaretPositionResolution();
  }
  if (affinity == TextAffinity::kUpstream && offset == current_offset.end + 1 &&
      cursor.Current().Style().NeedsTrailingSpace() &&
      cursor.Current().Style().IsCollapsibleWhiteSpace(
          mapping.GetText()[offset - 1])) {
    // |offset| is after soft line wrap, e.g. "abc |xyz".
    // See http://crbug.com/1183269 and |AdjustForSoftLineWrap()|
    return {ResolutionType::kResolved,
            {cursor, InlineCaretPositionType::kAtTextOffset, offset - 1}};
  }
  if (offset > current_offset.end &&
      !mapping.HasBidiControlCharactersOnly(end_offset, offset)) {
    return InlineCaretPositionResolution();
  }

  offset = std::max(offset, start_offset);
  offset = std::min(offset, end_offset);
  InlineCaretPosition candidate = {
      cursor, InlineCaretPositionType::kAtTextOffset, offset};

  // Offsets in the interior of a fragment can be resolved directly.
  if (offset > start_offset && offset < end_offset) {
    return {ResolutionType::kResolved, candidate};
  }

  if (offset == start_offset &&
      CanResolveInlineCaretPositionBeforeFragment(cursor, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  if (offset == end_offset && !cursor.Current().IsLineBreak() &&
      CanResolveInlineCaretPositionAfterFragment(cursor, affinity)) {
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
  std::optional<unsigned> maybe_offset_before =
      OffsetMapping::GetFor(before_node)->GetTextContentOffset(before_node);
  // We should have offset mapping for atomic inline boxes.
  DCHECK(maybe_offset_before.has_value());
  return *maybe_offset_before;
}

// Returns a |kFailed| resolution if |offset| doesn't belong to the atomic
// inline box fragment. Otherwise, return either |kFoundCandidate| or
// |kResolved| depending on |affinity|.
InlineCaretPositionResolution TryResolveInlineCaretPositionByBoxFragmentSide(
    const InlineCursor& cursor,
    unsigned offset,
    TextAffinity affinity) {
  const Node* const node = cursor.Current().GetNode();
  // There is no caret position at a pseudo or generated box side.
  if (!node || node->IsPseudoElement()) {
    // TODO(xiaochengh): This leads to false negatives for, e.g., RUBY, where an
    // anonymous wrapping inline block is created.
    return InlineCaretPositionResolution();
  }

  const unsigned offset_before = GetTextOffsetBefore(*node);
  const unsigned offset_after = offset_before + 1;
  // TODO(xiaochengh): Ignore bidi control characters before & after the box.
  if (offset != offset_before && offset != offset_after) {
    return InlineCaretPositionResolution();
  }
  const InlineCaretPositionType position_type =
      offset == offset_before ? InlineCaretPositionType::kBeforeBox
                              : InlineCaretPositionType::kAfterBox;
  InlineCaretPosition candidate{cursor, position_type, std::nullopt};

  if (offset == offset_before &&
      CanResolveInlineCaretPositionBeforeFragment(cursor, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  if (offset == offset_after &&
      CanResolveInlineCaretPositionAfterFragment(cursor, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  return {ResolutionType::kFoundCandidate, candidate};
}

InlineCaretPositionResolution TryResolveInlineCaretPositionWithFragment(
    const InlineCursor& cursor,
    unsigned offset,
    TextAffinity affinity) {
  if (cursor.Current().IsText()) {
    return TryResolveInlineCaretPositionInTextFragment(cursor, offset,
                                                       affinity);
  }
  if (cursor.Current().IsAtomicInline()) {
    return TryResolveInlineCaretPositionByBoxFragmentSide(cursor, offset,
                                                          affinity);
  }
  return InlineCaretPositionResolution();
}

bool NeedsBidiAdjustment(const InlineCaretPosition& caret_position) {
  if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled()) {
    return false;
  }
  if (caret_position.IsNull()) {
    return false;
  }
  if (caret_position.position_type != InlineCaretPositionType::kAtTextOffset) {
    return true;
  }
  DCHECK(caret_position.text_offset.has_value());
  const TextOffsetRange offset = caret_position.cursor.Current().TextOffset();
  const unsigned start_offset = offset.start;
  const unsigned end_offset = offset.end;
  DCHECK_GE(*caret_position.text_offset, start_offset);
  DCHECK_LE(*caret_position.text_offset, end_offset);
  // Bidi adjustment is needed only for caret positions at bidi boundaries.
  // Caret positions in the middle of a text fragment can't be at bidi
  // boundaries, and hence, don't need any adjustment.
  return *caret_position.text_offset == start_offset ||
         *caret_position.text_offset == end_offset;
}

InlineCaretPosition AdjustInlineCaretPositionForBidiText(
    const InlineCaretPosition& caret_position) {
  if (!NeedsBidiAdjustment(caret_position)) {
    return caret_position;
  }
  return BidiAdjustment::AdjustForInlineCaretPositionResolution(caret_position);
}

bool IsUpstreamAfterLineBreak(const InlineCaretPosition& caret_position) {
  if (caret_position.position_type != InlineCaretPositionType::kAtTextOffset) {
    return false;
  }

  DCHECK(caret_position.cursor.IsNotNull());
  DCHECK(caret_position.text_offset.has_value());

  if (!caret_position.cursor.Current().IsLineBreak()) {
    return false;
  }
  return *caret_position.text_offset ==
         caret_position.cursor.Current().TextEndOffset();
}

InlineCaretPosition BetterCandidateBetween(const InlineCaretPosition& current,
                                           const InlineCaretPosition& other,
                                           unsigned offset) {
  DCHECK(!other.IsNull());
  if (current.IsNull()) {
    return other;
  }

  // There shouldn't be too many cases where we have multiple candidates.
  // Make sure all of them are captured and handled here.

  // Only known case: either |current| or |other| is upstream after line break.
  DCHECK(current.ToPositionInDOMTreeWithAffinity().Affinity() ==
             TextAffinity::kUpstream ||
         other.ToPositionInDOMTreeWithAffinity().Affinity() ==
             TextAffinity::kUpstream);
  if (IsUpstreamAfterLineBreak(current)) {
    DCHECK(!IsUpstreamAfterLineBreak(other));
    return other;
  }
  return current;
}

InlineCaretPosition ComputeInlineCaretPositionAfterInline(
    const PositionWithAffinity& position_with_affinity) {
  const Position& position = position_with_affinity.GetPosition();
  const LayoutInline& layout_inline =
      *To<LayoutInline>(position.AnchorNode()->GetLayoutObject());

  InlineCursor cursor;
  cursor.MoveToIncludingCulledInline(layout_inline);
  // This DCHECK can fail with the <area> element.
  // DCHECK(cursor);
  if (!cursor) {
    return InlineCaretPosition();
  }
  InlineCursor line = cursor;
  line.MoveToContainingLine();
  DCHECK(line);

  if (IsLtr(line.Current().BaseDirection())) {
    cursor.MoveToVisualLastForSameLayoutObject();
  } else {
    cursor.MoveToVisualFirstForSameLayoutObject();
  }

  if (cursor.Current().IsText()) {
    const unsigned offset =
        line.Current().BaseDirection() == cursor.Current().ResolvedDirection()
            ? cursor.Current()->EndOffset()
            : cursor.Current()->StartOffset();
    return AdjustInlineCaretPositionForBidiText(
        {cursor, InlineCaretPositionType::kAtTextOffset, offset});
  }

  if (cursor.Current().IsAtomicInline()) {
    const InlineCaretPositionType type =
        line.Current().BaseDirection() == cursor.Current().ResolvedDirection()
            ? InlineCaretPositionType::kAfterBox
            : InlineCaretPositionType::kBeforeBox;
    return AdjustInlineCaretPositionForBidiText({cursor, type, std::nullopt});
  }

  return AdjustInlineCaretPositionForBidiText(
      {cursor, InlineCaretPositionType::kAfterBox, std::nullopt});
}
InlineCaretPosition ComputeInlineCaretPositionBeforeInline(
    const PositionWithAffinity& position_with_affinity) {
  const Position& position = position_with_affinity.GetPosition();
  const LayoutInline& layout_inline =
      *To<LayoutInline>(position.AnchorNode()->GetLayoutObject());

  InlineCursor cursor;
  cursor.MoveToIncludingCulledInline(layout_inline);
  // This DCHECK can fail with the <area> element.
  // DCHECK(cursor);
  if (!cursor) {
    return InlineCaretPosition();
  }
  InlineCursor line = cursor;
  line.MoveToContainingLine();
  DCHECK(line);

  if (IsLtr(line.Current().BaseDirection())) {
    cursor.MoveToVisualFirstForSameLayoutObject();
  } else {
    cursor.MoveToVisualLastForSameLayoutObject();
  }

  if (cursor.Current().IsText()) {
    const unsigned offset =
        line.Current().BaseDirection() == cursor.Current().ResolvedDirection()
            ? cursor.Current()->StartOffset()
            : cursor.Current()->EndOffset();
    return AdjustInlineCaretPositionForBidiText(
        {cursor, InlineCaretPositionType::kAtTextOffset, offset});
  }

  if (cursor.Current().IsAtomicInline()) {
    const InlineCaretPositionType type =
        line.Current().BaseDirection() == cursor.Current().ResolvedDirection()
            ? InlineCaretPositionType::kBeforeBox
            : InlineCaretPositionType::kAfterBox;
    return AdjustInlineCaretPositionForBidiText({cursor, type, std::nullopt});
  }

  return AdjustInlineCaretPositionForBidiText(
      {cursor, InlineCaretPositionType::kBeforeBox, std::nullopt});
}

}  // namespace

// The main function for compute an InlineCaretPosition. See the comments at the
// top of this file for details.
InlineCaretPosition ComputeInlineCaretPosition(const LayoutBlockFlow& context,
                                               unsigned offset,
                                               TextAffinity affinity,
                                               const LayoutText* layout_text) {
  InlineCursor cursor(context);

  InlineCaretPosition candidate;
  if (layout_text && layout_text->HasInlineFragments()) {
    cursor.MoveTo(*layout_text);
  }
  for (; cursor; cursor.MoveToNextIncludingFragmentainer()) {
    const InlineCaretPositionResolution resolution =
        TryResolveInlineCaretPositionWithFragment(cursor, offset, affinity);

    if (resolution.type == ResolutionType::kFailed) {
      continue;
    }

    // TODO(xiaochengh): Handle caret poisition in empty container (e.g. empty
    // line box).

    if (resolution.type == ResolutionType::kResolved) {
      candidate = resolution.caret_position;
      if (!layout_text ||
          candidate.cursor.Current().GetLayoutObject() == layout_text) {
        return AdjustInlineCaretPositionForBidiText(resolution.caret_position);
      }
      continue;
    }

    DCHECK_EQ(ResolutionType::kFoundCandidate, resolution.type);
    candidate =
        BetterCandidateBetween(candidate, resolution.caret_position, offset);
  }

  return AdjustInlineCaretPositionForBidiText(candidate);
}

InlineCaretPosition ComputeInlineCaretPosition(
    const PositionWithAffinity& position_with_affinity) {
  const Position& position = position_with_affinity.GetPosition();

  if (position.IsNull()) {
    return InlineCaretPosition();
  }

  const LayoutObject* layout_object = position.AnchorNode()->GetLayoutObject();
  if (!layout_object || !layout_object->IsInLayoutNGInlineFormattingContext()) {
    return InlineCaretPosition();
  }

  if (layout_object->IsLayoutInline()) {
    if (position.IsBeforeAnchor()) {
      return ComputeInlineCaretPositionBeforeInline(position_with_affinity);
    }
    if (position.IsAfterAnchor()) {
      return ComputeInlineCaretPositionAfterInline(position_with_affinity);
    }
    NOTREACHED_IN_MIGRATION()
        << "Caller should not pass a position inside inline: " << position;
    return InlineCaretPosition();
  }

  LayoutBlockFlow* const context = NGInlineFormattingContextOf(position);
  if (!context) {
    // We reach here for empty <div>[1].
    // [1] third_party/blink/web_tests/editing/caret/caret-in-inline-block.html
    return InlineCaretPosition();
  }

  const OffsetMapping* const mapping = InlineNode::GetOffsetMapping(context);
  if (!mapping) {
    // A block containing the position might be display-locked.
    // See editing/caret/caret-display-locked-crash.html
    return InlineCaretPosition();
  }
  const std::optional<unsigned> maybe_offset =
      mapping->GetTextContentOffset(position);
  if (!maybe_offset.has_value()) {
    // We can reach here with empty text nodes.
    if (auto* data = DynamicTo<Text>(position.AnchorNode())) {
      DCHECK_EQ(data->length(), 0u);
    } else {
      // TODO(xiaochengh): Investigate if we reach here.
      NOTREACHED_IN_MIGRATION();
      return InlineCaretPosition();
    }
  }

  const LayoutText* const layout_text =
      position.IsOffsetInAnchor() && IsA<Text>(position.AnchorNode())
          ? To<LayoutText>(AssociatedLayoutObjectOf(
                *position.AnchorNode(), position.OffsetInContainerNode()))
          : nullptr;

  const unsigned offset = maybe_offset.value_or(0);
  const TextAffinity affinity = position_with_affinity.Affinity();
  // For upstream position, we use offset before ZWS to distinguish downstream
  // and upstream position when line breaking before ZWS.
  // "    Zabc" where "Z" represents zero-width-space.
  // See AccessibilitySelectionTest.FromCurrentSelectionInTextareaWithAffinity
  const unsigned adjusted_offset =
      affinity == TextAffinity::kUpstream && offset &&
              mapping->GetText()[offset - 1] == kZeroWidthSpaceCharacter
          ? offset - 1
          : offset;
  return ComputeInlineCaretPosition(*context, adjusted_offset, affinity,
                                    layout_text);
}

Position InlineCaretPosition::ToPositionInDOMTree() const {
  return ToPositionInDOMTreeWithAffinity().GetPosition();
}

PositionWithAffinity InlineCaretPosition::ToPositionInDOMTreeWithAffinity()
    const {
  if (IsNull()) {
    return PositionWithAffinity();
  }
  switch (position_type) {
    case InlineCaretPositionType::kBeforeBox:
      if (const Node* node = cursor.Current().GetNode()) {
        return PositionWithAffinity(Position::BeforeNode(*node),
                                    TextAffinity::kDownstream);
      }
      return PositionWithAffinity();
    case InlineCaretPositionType::kAfterBox:
      if (const Node* node = cursor.Current().GetNode()) {
        return PositionWithAffinity(Position::AfterNode(*node),
                                    TextAffinity::kUpstreamIfPossible);
      }
      return PositionWithAffinity();
    case InlineCaretPositionType::kAtTextOffset:
      // In case of ::first-letter, |cursor.Current().GetNode()| is null.
      DCHECK(text_offset.has_value());
      const OffsetMapping* mapping =
          OffsetMapping::GetFor(cursor.Current().GetLayoutObject());
      if (!mapping) {
        // TODO(yosin): We're not sure why |mapping| is |nullptr|. It seems
        // we are attempt to use destroyed/moved |FragmentItem|.
        // See http://crbug.com/1145514
        DUMP_WILL_BE_NOTREACHED()
            << cursor << " " << cursor.Current().GetLayoutObject();
        return PositionWithAffinity();
      }
      const TextAffinity affinity =
          *text_offset == cursor.Current().TextEndOffset()
              ? TextAffinity::kUpstreamIfPossible
              : TextAffinity::kDownstream;
      const Position position = affinity == TextAffinity::kDownstream
                                    ? mapping->GetLastPosition(*text_offset)
                                    : mapping->GetFirstPosition(*text_offset);
      if (position.IsNull()) {
        return PositionWithAffinity();
      }
      return PositionWithAffinity(position, affinity);
  }
  NOTREACHED_IN_MIGRATION();
  return PositionWithAffinity();
}

}  // namespace blink
