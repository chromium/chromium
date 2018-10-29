// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"

#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

namespace blink {

namespace {

void AssertValidPositionForCaretPositionComputation(
    const PositionWithAffinity& position) {
#if DCHECK_IS_ON()
  DCHECK(NGOffsetMapping::AcceptsPosition(position.GetPosition()));
  const LayoutObject* layout_object = position.AnchorNode()->GetLayoutObject();
  DCHECK(layout_object);
  DCHECK(layout_object->IsText() || layout_object->IsAtomicInlineLevel());
#endif
}

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
  ResolutionType type = ResolutionType::kFailed;
  NGCaretPosition caret_position;
};

bool CanResolveCaretPositionBeforeFragment(const NGPaintFragment& fragment,
                                           TextAffinity affinity) {
  if (affinity == TextAffinity::kDownstream)
    return true;
  const NGPaintFragment* current_line_paint = fragment.ContainerLineBox();
  const NGPhysicalLineBoxFragment& current_line =
      ToNGPhysicalLineBoxFragment(current_line_paint->PhysicalFragment());
  // A fragment after line wrap must be the first logical leaf in its line.
  if (&fragment.PhysicalFragment() != current_line.FirstLogicalLeaf())
    return true;
  const NGPaintFragment* last_line_paint =
      NGPaintFragmentTraversal::PreviousLineOf(*current_line_paint);
  return !last_line_paint ||
         !ToNGPhysicalLineBoxFragment(last_line_paint->PhysicalFragment())
              .HasSoftWrapToNextLine();
}

bool CanResolveCaretPositionAfterFragment(const NGPaintFragment& fragment,
                                          TextAffinity affinity) {
  if (affinity == TextAffinity::kUpstream)
    return true;
  const NGPaintFragment* current_line_paint = fragment.ContainerLineBox();
  const NGPhysicalLineBoxFragment& current_line =
      ToNGPhysicalLineBoxFragment(current_line_paint->PhysicalFragment());
  // A fragment before line wrap must be the last logical leaf in its line.
  if (&fragment.PhysicalFragment() != current_line.LastLogicalLeaf())
    return true;
  return !current_line.HasSoftWrapToNextLine();
}

// Returns a |kFailed| resolution if |offset| doesn't belong to the text
// fragment. Otherwise, return either |kFoundCandidate| or |kResolved| depending
// on |affinity|.
CaretPositionResolution TryResolveCaretPositionInTextFragment(
    const NGPaintFragment& paint_fragment,
    unsigned offset,
    TextAffinity affinity) {
  DCHECK(paint_fragment.PhysicalFragment().IsText());
  const NGPhysicalTextFragment& fragment =
      ToNGPhysicalTextFragment(paint_fragment.PhysicalFragment());
  if (fragment.IsAnonymousText())
    return CaretPositionResolution();

  const NGOffsetMapping& mapping =
      *NGOffsetMapping::GetFor(paint_fragment.GetLayoutObject());

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
  if (offset < fragment.StartOffset() &&
      !mapping.HasBidiControlCharactersOnly(offset, fragment.StartOffset()))
    return CaretPositionResolution();
  if (offset > fragment.EndOffset() &&
      !mapping.HasBidiControlCharactersOnly(fragment.EndOffset(), offset))
    return CaretPositionResolution();

  offset = std::max(offset, fragment.StartOffset());
  offset = std::min(offset, fragment.EndOffset());
  NGCaretPosition candidate = {&paint_fragment,
                               NGCaretPositionType::kAtTextOffset, offset};

  // Offsets in the interior of a fragment can be resolved directly.
  if (offset > fragment.StartOffset() && offset < fragment.EndOffset())
    return {ResolutionType::kResolved, candidate};

  if (offset == fragment.StartOffset() &&
      CanResolveCaretPositionBeforeFragment(paint_fragment, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  if (offset == fragment.EndOffset() && !fragment.IsLineBreak() &&
      CanResolveCaretPositionAfterFragment(paint_fragment, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  // We may have a better candidate
  return {ResolutionType::kFoundCandidate, candidate};
}

unsigned GetTextOffsetBefore(const NGPhysicalFragment& fragment) {
  // TODO(xiaochengh): Design more straightforward way to get text offset of
  // atomic inline box.
  DCHECK(fragment.IsAtomicInline());
  const Node* node = fragment.GetNode();
  DCHECK(node);
  const Position before_node = Position::BeforeNode(*node);
  base::Optional<unsigned> maybe_offset_before =
      NGOffsetMapping::GetFor(before_node)->GetTextContentOffset(before_node);
  // We should have offset mapping for atomic inline boxes.
  DCHECK(maybe_offset_before.has_value());
  return maybe_offset_before.value();
}

// Returns a |kFailed| resolution if |offset| doesn't belong to the atomic
// inline box fragment. Otherwise, return either |kFoundCandidate| or
// |kResolved| depending on |affinity|.
CaretPositionResolution TryResolveCaretPositionByBoxFragmentSide(
    const NGPaintFragment& fragment,
    unsigned offset,
    TextAffinity affinity) {
  // There is no caret position at a pseudo or generated box side.
  if (!fragment.GetNode() || fragment.GetNode()->IsPseudoElement()) {
    // TODO(xiaochengh): This leads to false negatives for, e.g., RUBY, where an
    // anonymous wrapping inline block is created.
    return CaretPositionResolution();
  }

  const unsigned offset_before =
      GetTextOffsetBefore(fragment.PhysicalFragment());
  const unsigned offset_after = offset_before + 1;
  // TODO(xiaochengh): Ignore bidi control characters before & after the box.
  if (offset != offset_before && offset != offset_after)
    return CaretPositionResolution();
  const NGCaretPositionType position_type =
      offset == offset_before ? NGCaretPositionType::kBeforeBox
                              : NGCaretPositionType::kAfterBox;
  NGCaretPosition candidate{&fragment, position_type, base::nullopt};

  if (offset == offset_before &&
      CanResolveCaretPositionBeforeFragment(fragment, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  if (offset == offset_after &&
      CanResolveCaretPositionAfterFragment(fragment, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  return {ResolutionType::kFoundCandidate, candidate};
}

CaretPositionResolution TryResolveCaretPositionWithFragment(
    const NGPaintFragment& paint_fragment,
    unsigned offset,
    TextAffinity affinity) {
  const NGPhysicalFragment& fragment = paint_fragment.PhysicalFragment();
  if (fragment.IsText()) {
    return TryResolveCaretPositionInTextFragment(paint_fragment, offset,
                                                 affinity);
  }
  if (fragment.IsBox() && fragment.IsAtomicInline()) {
    return TryResolveCaretPositionByBoxFragmentSide(paint_fragment, offset,
                                                    affinity);
  }
  return CaretPositionResolution();
}

bool NeedsBidiAdjustment(const NGCaretPosition& caret_position) {
  if (caret_position.IsNull())
    return false;
  if (caret_position.position_type != NGCaretPositionType::kAtTextOffset)
    return true;
  DCHECK(caret_position.text_offset.has_value());
  DCHECK(caret_position.fragment->PhysicalFragment().IsText());
  const NGPhysicalTextFragment& text_fragment =
      ToNGPhysicalTextFragment(caret_position.fragment->PhysicalFragment());
  DCHECK_GE(*caret_position.text_offset, text_fragment.StartOffset());
  DCHECK_LE(*caret_position.text_offset, text_fragment.EndOffset());
  // Bidi adjustment is needed only for caret positions at bidi boundaries.
  // Caret positions in the middle of a text fragment can't be at bidi
  // boundaries, and hence, don't need any adjustment.
  return *caret_position.text_offset == text_fragment.StartOffset() ||
         *caret_position.text_offset == text_fragment.EndOffset();
}

NGCaretPosition AdjustCaretPositionForBidiText(
    const NGCaretPosition& caret_position) {
  if (!NeedsBidiAdjustment(caret_position))
    return caret_position;
  return BidiAdjustment::AdjustForCaretPositionResolution(caret_position);
}

}  // namespace

// The main function for compute an NGCaretPosition. See the comments at the top
// of this file for details.
NGCaretPosition ComputeNGCaretPosition(const LayoutBlockFlow& context,
                                       unsigned offset,
                                       TextAffinity affinity) {
  const NGPaintFragment* root_fragment = context.PaintFragment();
  DCHECK(root_fragment) << "no paint fragment on layout object " << &context;

  NGCaretPosition candidate;
  for (const auto& child :
       NGPaintFragmentTraversal::InlineDescendantsOf(*root_fragment)) {
    const CaretPositionResolution resolution =
        TryResolveCaretPositionWithFragment(*child.fragment, offset, affinity);

    if (resolution.type == ResolutionType::kFailed)
      continue;

    // TODO(xiaochengh): Handle caret poisition in empty container (e.g. empty
    // line box).

    if (resolution.type == ResolutionType::kResolved)
      return AdjustCaretPositionForBidiText(resolution.caret_position);

    DCHECK_EQ(ResolutionType::kFoundCandidate, resolution.type);
    // TODO(xiaochengh): We are not sure if we can ever find multiple
    // candidates. Handle it once reached.
    DCHECK(candidate.IsNull());
    candidate = resolution.caret_position;
  }

  return AdjustCaretPositionForBidiText(candidate);
}

NGCaretPosition ComputeNGCaretPosition(const PositionWithAffinity& position) {
  AssertValidPositionForCaretPositionComputation(position);
  LayoutBlockFlow* context =
      NGInlineFormattingContextOf(position.GetPosition());
  if (!context)
    return NGCaretPosition();

  const NGOffsetMapping* mapping =
      NGOffsetMapping::GetForContainingBlockFlow(context);
  DCHECK(mapping);
  const base::Optional<unsigned> maybe_offset =
      mapping->GetTextContentOffset(position.GetPosition());
  if (!maybe_offset.has_value()) {
    // TODO(xiaochengh): Investigate if we reach here.
    NOTREACHED();
    return NGCaretPosition();
  }

  const unsigned offset = maybe_offset.value();
  const TextAffinity affinity = position.Affinity();
  return ComputeNGCaretPosition(*context, offset, affinity);
}

Position NGCaretPosition::ToPositionInDOMTree() const {
  return ToPositionInDOMTreeWithAffinity().GetPosition();
}

PositionWithAffinity NGCaretPosition::ToPositionInDOMTreeWithAffinity() const {
  if (!fragment)
    return PositionWithAffinity();
  switch (position_type) {
    case NGCaretPositionType::kBeforeBox:
      if (!fragment->GetNode())
        return PositionWithAffinity();
      return PositionWithAffinity(Position::BeforeNode(*fragment->GetNode()),
                                  TextAffinity::kDownstream);
    case NGCaretPositionType::kAfterBox:
      if (!fragment->GetNode())
        return PositionWithAffinity();
      return PositionWithAffinity(Position::AfterNode(*fragment->GetNode()),
                                  TextAffinity::kUpstreamIfPossible);
    case NGCaretPositionType::kAtTextOffset:
      DCHECK(text_offset.has_value());
      const NGOffsetMapping* mapping =
          NGOffsetMapping::GetFor(fragment->GetLayoutObject());
      const Position position = mapping->GetFirstPosition(*text_offset);
      if (position.IsNull())
        return PositionWithAffinity();
      const NGPhysicalTextFragment& text_fragment =
          ToNGPhysicalTextFragment(fragment->PhysicalFragment());
      const TextAffinity affinity =
          text_offset.value() == text_fragment.EndOffset()
              ? TextAffinity::kUpstreamIfPossible
              : TextAffinity::kDownstream;
      return PositionWithAffinity(position, affinity);
  }
  NOTREACHED();
  return PositionWithAffinity();
}

}  // namespace blink
