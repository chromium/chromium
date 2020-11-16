// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/optional.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_early_break.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace {

bool HasLineEvenIfEmpty(LayoutBox* box) {
  LayoutBlockFlow* const block_flow = DynamicTo<LayoutBlockFlow>(box);
  if (!block_flow)
    return false;
  // Note: |block_flow->NeedsCollectInline()| is true after removing all
  // children from block[1].
  // [1] editing/inserting/insert_after_delete.html
  LayoutObject* const child = GetLayoutObjectForFirstChildNode(block_flow);
  if (!child) {
    // Note: |block_flow->ChildrenInline()| can be both true or false:
    //  - true: just after construction, <div></div>
    //  - true: one of child is inline them remove all, <div>abc</div>
    //  - false: all children are block then remove all, <div><p></p></div>
    return block_flow->HasLineIfEmpty();
  }
  if (!AreNGBlockFlowChildrenInline(block_flow))
    return false;
  return NGInlineNode(block_flow).HasLineEvenIfEmpty();
}

LogicalOffset CenterBlockChild(LogicalOffset offset,
                               LayoutUnit available_block_size,
                               LayoutUnit child_block_size) {
  if (available_block_size == child_block_size)
    return offset;
  // We don't clamp a negative difference to zero. We'd like to center the
  // child even if its taller than the container.
  LayoutUnit block_size_diff = available_block_size - child_block_size;
  offset.block_offset += block_size_diff / 2 + LayoutMod(block_size_diff, 2);
  return offset;
}

inline scoped_refptr<const NGLayoutResult> LayoutBlockChild(
    const NGConstraintSpace& space,
    const NGBreakToken* break_token,
    const NGEarlyBreak* early_break,
    NGBlockNode* node) {
  const NGEarlyBreak* early_break_in_child = nullptr;
  if (UNLIKELY(early_break && early_break->Type() == NGEarlyBreak::kBlock &&
               early_break->BlockNode() == *node)) {
    // We're entering a child that we know that we're going to break inside, and
    // even where to break. Look inside, and pass the inner breakpoint to
    // layout.
    early_break_in_child = early_break->BreakInside();
    // If there's no break inside, we should already have broken before this
    // child.
    DCHECK(early_break_in_child);
  }
  return node->Layout(space, To<NGBlockBreakToken>(break_token),
                      early_break_in_child);
}

inline scoped_refptr<const NGLayoutResult> LayoutInflow(
    const NGConstraintSpace& space,
    const NGBreakToken* break_token,
    const NGEarlyBreak* early_break,
    NGLayoutInputNode* node,
    NGInlineChildLayoutContext* context) {
  if (auto* inline_node = DynamicTo<NGInlineNode>(node))
    return inline_node->Layout(space, break_token, context);
  return LayoutBlockChild(space, break_token, early_break,
                          To<NGBlockNode>(node));
}

NGAdjoiningObjectTypes ToAdjoiningObjectTypes(EClear clear) {
  switch (clear) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case EClear::kNone:
      return kAdjoiningNone;
    case EClear::kLeft:
      return kAdjoiningFloatLeft;
    case EClear::kRight:
      return kAdjoiningFloatRight;
    case EClear::kBoth:
      return kAdjoiningFloatBoth;
  };
}

// Return true if a child is to be cleared past adjoining floats. These are
// floats that would otherwise (if 'clear' were 'none') be pulled down by the
// BFC block offset of the child. If the child is to clear floats, though, we
// obviously need separate the child from the floats and move it past them,
// since that's what clearance is all about. This means that if we have any such
// floats to clear, we know for sure that we get clearance, even before layout.
inline bool HasClearancePastAdjoiningFloats(
    NGAdjoiningObjectTypes adjoining_object_types,
    const ComputedStyle& child_style,
    const ComputedStyle& cb_style) {
  return ToAdjoiningObjectTypes(child_style.Clear(cb_style)) &
         adjoining_object_types;
}

// Adjust BFC block offset for clearance, if applicable. Return true of
// clearance was applied.
//
// Clearance applies either when the BFC block offset calculated simply isn't
// past all relevant floats, *or* when we have already determined that we're
// directly preceded by clearance.
//
// The latter is the case when we need to force ourselves past floats that would
// otherwise be adjoining, were it not for the predetermined clearance.
// Clearance inhibits margin collapsing and acts as spacing before the
// block-start margin of the child. It needs to be exactly what takes the
// block-start border edge of the cleared block adjacent to the block-end outer
// edge of the "bottommost" relevant float.
//
// We cannot reliably calculate the actual clearance amount at this point,
// because 1) this block right here may actually be a descendant of the block
// that is to be cleared, and 2) we may not yet have separated the margin before
// and after the clearance. None of this matters, though, because we know where
// to place this block if clearance applies: exactly at the ConstraintSpace's
// ClearanceOffset().
bool ApplyClearance(const NGConstraintSpace& constraint_space,
                    LayoutUnit* bfc_block_offset) {
  if (constraint_space.HasClearanceOffset() &&
      *bfc_block_offset < constraint_space.ClearanceOffset()) {
    *bfc_block_offset = constraint_space.ClearanceOffset();
    return true;
  }
  return false;
}

LayoutUnit LogicalFromBfcLineOffset(LayoutUnit child_bfc_line_offset,
                                    LayoutUnit parent_bfc_line_offset,
                                    LayoutUnit child_inline_size,
                                    LayoutUnit parent_inline_size,
                                    TextDirection direction) {
  // We need to respect the current text direction to calculate the logical
  // offset correctly.
  LayoutUnit relative_line_offset =
      child_bfc_line_offset - parent_bfc_line_offset;

  LayoutUnit inline_offset =
      direction == TextDirection::kLtr
          ? relative_line_offset
          : parent_inline_size - relative_line_offset - child_inline_size;

  return inline_offset;
}

LogicalOffset LogicalFromBfcOffsets(const NGBfcOffset& child_bfc_offset,
                                    const NGBfcOffset& parent_bfc_offset,
                                    LayoutUnit child_inline_size,
                                    LayoutUnit parent_inline_size,
                                    TextDirection direction) {
  LayoutUnit inline_offset = LogicalFromBfcLineOffset(
      child_bfc_offset.line_offset, parent_bfc_offset.line_offset,
      child_inline_size, parent_inline_size, direction);

  return {inline_offset,
          child_bfc_offset.block_offset - parent_bfc_offset.block_offset};
}

inline bool IsEarlyBreakpoint(const NGEarlyBreak& breakpoint,
                              const NGBoxFragmentBuilder& builder,
                              NGLayoutInputNode child) {
  if (breakpoint.Type() == NGEarlyBreak::kLine)
    return child.IsInline() && breakpoint.LineNumber() == builder.LineCount();
  if (breakpoint.IsBreakBefore())
    return breakpoint.BlockNode() == child;
  return false;
}

}  // namespace

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      previous_result_(params.previous_result),
      fit_all_lines_(false),
      is_resuming_(IsResumingLayout(params.break_token)),
      abort_when_bfc_block_offset_updated_(false),
      has_processed_first_child_(false),
      ignore_line_clamp_(false),
      is_line_clamp_context_(params.space.IsLineClampContext()),
      lines_until_clamp_(params.space.LinesUntilClamp()),
      exclusion_space_(params.space.ExclusionSpace()),
      early_break_(params.early_break) {}

// Define the destructor here, so that we can forward-declare more in the
// header.
NGBlockLayoutAlgorithm::~NGBlockLayoutAlgorithm() = default;

void NGBlockLayoutAlgorithm::SetBoxType(NGPhysicalFragment::NGBoxType type) {
  container_builder_.SetBoxType(type);
}

MinMaxSizesResult NGBlockLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  if (auto result =
          CalculateMinMaxSizesIgnoringChildren(node_, BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  bool depends_on_percentage_block_size = false;

  const TextDirection direction = Style().Direction();
  LayoutUnit float_left_inline_size = input.float_left_inline_size;
  LayoutUnit float_right_inline_size = input.float_right_inline_size;

  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    // We don't check IsRubyText() here intentionally. RubyText width should
    // affect this width.
    if (child.IsOutOfFlowPositioned() ||
        (child.IsColumnSpanAll() && ConstraintSpace().IsInColumnBfc()) ||
        child.IsTextControlPlaceholder())
      continue;

    const ComputedStyle& child_style = child.Style();
    const EClear child_clear = child_style.Clear(Style());
    bool child_is_new_fc = child.CreatesNewFormattingContext();

    // Conceptually floats and a single new-FC would just get positioned on a
    // single "line". If there is a float/new-FC with clearance, this creates a
    // new "line", resetting the appropriate float size trackers.
    //
    // Both of the float size trackers get reset for anything that isn't a float
    // (inflow and new-FC) at the end of the loop, as this creates a new "line".
    if (child.IsFloating() || child_is_new_fc) {
      LayoutUnit float_inline_size =
          float_left_inline_size + float_right_inline_size;

      if (child_clear != EClear::kNone)
        sizes.max_size = std::max(sizes.max_size, float_inline_size);

      if (child_clear == EClear::kBoth || child_clear == EClear::kLeft)
        float_left_inline_size = LayoutUnit();

      if (child_clear == EClear::kBoth || child_clear == EClear::kRight)
        float_right_inline_size = LayoutUnit();
    }

    MinMaxSizesInput child_input(input.percentage_resolution_block_size,
                                 input.type);
    if (child.IsInline() || child.IsAnonymousBlock()) {
      child_input.float_left_inline_size = float_left_inline_size;
      child_input.float_right_inline_size = float_right_inline_size;
    }

    MinMaxSizesResult child_result;
    if (child.IsInline()) {
      // From |NGBlockLayoutAlgorithm| perspective, we can handle |NGInlineNode|
      // almost the same as |NGBlockNode|, because an |NGInlineNode| includes
      // all inline nodes following |child| and their descendants, and produces
      // an anonymous box that contains all line boxes.
      // |NextSibling| returns the next block sibling, or nullptr, skipping all
      // following inline siblings and descendants.
      child_result =
          child.ComputeMinMaxSizes(Style().GetWritingMode(), child_input);
    } else {
      child_result = ComputeMinAndMaxContentContribution(
          Style(), To<NGBlockNode>(child), child_input);
    }
    DCHECK_LE(child_result.sizes.min_size, child_result.sizes.max_size)
        << child.ToString();

    // Determine the max inline contribution of the child.
    NGBoxStrut margins = ComputeMinMaxMargins(Style(), child);
    LayoutUnit max_inline_contribution;

    if (child.IsFloating()) {
      // A float adds to its inline size to the current "line". The new max
      // inline contribution is just the sum of all the floats on that "line".
      LayoutUnit float_inline_size =
          child_result.sizes.max_size + margins.InlineSum();

      // float_inline_size is negative when the float is completely outside of
      // the content area, by e.g., negative margins. Such floats do not affect
      // the content size.
      if (float_inline_size > 0) {
        if (child_style.Floating(Style()) == EFloat::kLeft)
          float_left_inline_size += float_inline_size;
        else
          float_right_inline_size += float_inline_size;
      }

      max_inline_contribution =
          float_left_inline_size + float_right_inline_size;
    } else if (child_is_new_fc) {
      // As floats are line relative, we perform the margin calculations in the
      // line relative coordinate system as well.
      LayoutUnit margin_line_left = margins.LineLeft(direction);
      LayoutUnit margin_line_right = margins.LineRight(direction);

      // line_left_inset and line_right_inset are the "distance" from their
      // respective edges of the parent that the new-FC would take. If the
      // margin is positive the inset is just whichever of the floats inline
      // size and margin is larger, and if negative it just subtracts from the
      // float inline size.
      LayoutUnit line_left_inset =
          margin_line_left > LayoutUnit()
              ? std::max(float_left_inline_size, margin_line_left)
              : float_left_inline_size + margin_line_left;

      LayoutUnit line_right_inset =
          margin_line_right > LayoutUnit()
              ? std::max(float_right_inline_size, margin_line_right)
              : float_right_inline_size + margin_line_right;

      // The order of operations is important here.
      // If child_result.sizes.max_size is saturated, adding the insets
      // sequentially can result in an DCHECK.
      max_inline_contribution =
          child_result.sizes.max_size + (line_left_inset + line_right_inset);
    } else {
      // This is just a standard inflow child.
      max_inline_contribution =
          child_result.sizes.max_size + margins.InlineSum();
    }
    sizes.max_size = std::max(sizes.max_size, max_inline_contribution);

    // The min inline contribution just assumes that floats are all on their own
    // "line".
    LayoutUnit min_inline_contribution =
        child_result.sizes.min_size + margins.InlineSum();
    sizes.min_size = std::max(sizes.min_size, min_inline_contribution);

    depends_on_percentage_block_size |=
        child_result.depends_on_percentage_block_size;

    // Anything that isn't a float will create a new "line" resetting the float
    // size trackers.
    if (!child.IsFloating()) {
      float_left_inline_size = LayoutUnit();
      float_right_inline_size = LayoutUnit();
    }
  }

  DCHECK_GE(sizes.min_size, LayoutUnit());
  DCHECK_LE(sizes.min_size, sizes.max_size) << Node().ToString();

  sizes += BorderScrollbarPadding().InlineSum();
  return {sizes, depends_on_percentage_block_size};
}

LogicalOffset NGBlockLayoutAlgorithm::CalculateLogicalOffset(
    const NGFragment& fragment,
    LayoutUnit child_bfc_line_offset,
    const base::Optional<LayoutUnit>& child_bfc_block_offset) {
  LayoutUnit inline_size = container_builder_.Size().inline_size;
  TextDirection direction = ConstraintSpace().Direction();

  if (child_bfc_block_offset && container_builder_.BfcBlockOffset()) {
    return LogicalFromBfcOffsets(
        {child_bfc_line_offset, *child_bfc_block_offset}, ContainerBfcOffset(),
        fragment.InlineSize(), inline_size, direction);
  }

  LayoutUnit inline_offset = LogicalFromBfcLineOffset(
      child_bfc_line_offset, container_builder_.BfcLineOffset(),
      fragment.InlineSize(), inline_size, direction);

  // If we've reached here, either the parent, or the child don't have a BFC
  // block-offset yet. Children in this situation are always placed at a
  // logical block-offset of zero.
  return {inline_offset, LayoutUnit()};
}

scoped_refptr<const NGLayoutResult> NGBlockLayoutAlgorithm::Layout() {
  scoped_refptr<const NGLayoutResult> result;
  // Inline children require an inline child layout context to be
  // passed between siblings. We want to stack-allocate that one, but
  // only on demand, as it's quite big.
  NGLayoutInputNode first_child(nullptr);
  if (Node().IsInlineFormattingContextRoot(&first_child))
    result = LayoutWithInlineChildLayoutContext(first_child);
  else
    result = Layout(nullptr);
  if (UNLIKELY(result->Status() == NGLayoutResult::kNeedsEarlierBreak)) {
    // If we found a good break somewhere inside this block, re-layout and break
    // at that location.
    DCHECK(!early_break_);
    DCHECK(result->GetEarlyBreak());
    return RelayoutAndBreakEarlier(*result->GetEarlyBreak());
  } else if (UNLIKELY(result->Status() ==
                      NGLayoutResult::
                          kNeedsRelayoutWithNoForcedTruncateAtLineClamp)) {
    DCHECK(!ignore_line_clamp_);
    return RelayoutIgnoringLineClamp();
  }
  return result;
}

NOINLINE scoped_refptr<const NGLayoutResult>
NGBlockLayoutAlgorithm::LayoutWithInlineChildLayoutContext(
    const NGLayoutInputNode& first_child) {
  NGInlineChildLayoutContext context;
  if (!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return Layout(&context);
  return LayoutWithItemsBuilder(To<NGInlineNode>(first_child), &context);
}

NOINLINE scoped_refptr<const NGLayoutResult>
NGBlockLayoutAlgorithm::LayoutWithItemsBuilder(
    const NGInlineNode& first_child,
    NGInlineChildLayoutContext* context) {
  NGFragmentItemsBuilder items_builder(
      first_child, container_builder_.GetWritingDirection());
  container_builder_.SetItemsBuilder(&items_builder);
  context->SetItemsBuilder(&items_builder);
  scoped_refptr<const NGLayoutResult> result = Layout(context);
  // Ensure stack-allocated |NGFragmentItemsBuilder| is not used anymore.
  // TODO(kojii): Revisit when the storage of |NGFragmentItemsBuilder| is
  // finalized.
  container_builder_.SetItemsBuilder(nullptr);
  context->SetItemsBuilder(nullptr);
  return result;
}

NOINLINE scoped_refptr<const NGLayoutResult>
NGBlockLayoutAlgorithm::RelayoutAndBreakEarlier(
    const NGEarlyBreak& breakpoint) {
  NGLayoutAlgorithmParams params(Node(),
                                 container_builder_.InitialFragmentGeometry(),
                                 ConstraintSpace(), BreakToken(), &breakpoint);
  NGBlockLayoutAlgorithm algorithm_with_break(params);
  NGBoxFragmentBuilder& new_builder = algorithm_with_break.container_builder_;
  new_builder.SetBoxType(container_builder_.BoxType());
  // We're not going to run out of space in the next layout pass, since we're
  // breaking earlier, so no space shortage will be detected. Repeat what we
  // found in this pass.
  new_builder.PropagateSpaceShortage(container_builder_.MinimalSpaceShortage());
  return algorithm_with_break.Layout();
}

NOINLINE scoped_refptr<const NGLayoutResult>
NGBlockLayoutAlgorithm::RelayoutIgnoringLineClamp() {
  NGLayoutAlgorithmParams params(Node(),
                                 container_builder_.InitialFragmentGeometry(),
                                 ConstraintSpace(), BreakToken(), nullptr);
  NGBlockLayoutAlgorithm algorithm_ignoring_line_clamp(params);
  algorithm_ignoring_line_clamp.ignore_line_clamp_ = true;
  NGBoxFragmentBuilder& new_builder =
      algorithm_ignoring_line_clamp.container_builder_;
  new_builder.SetBoxType(container_builder_.BoxType());
  return algorithm_ignoring_line_clamp.Layout();
}

inline scoped_refptr<const NGLayoutResult> NGBlockLayoutAlgorithm::Layout(
    NGInlineChildLayoutContext* inline_child_layout_context) {
  child_percentage_size_ = CalculateChildPercentageSize(
      ConstraintSpace(), Node(), ChildAvailableSize());
  replaced_child_percentage_size_ = CalculateReplacedChildPercentageSize(
      ConstraintSpace(), Node(), ChildAvailableSize(), BorderScrollbarPadding(),
      BorderPadding());

  if (ConstraintSpace().IsLegacyTableCell())
    container_builder_.AdjustBorderScrollbarPaddingForTableCell();

  DCHECK_EQ(!!inline_child_layout_context,
            Node().IsInlineFormattingContextRoot());
  container_builder_.SetIsInlineFormattingContext(inline_child_layout_context);

  container_builder_.SetBfcLineOffset(
      ConstraintSpace().BfcOffset().line_offset);

  if (NGAdjoiningObjectTypes adjoining_object_types =
          ConstraintSpace().AdjoiningObjectTypes()) {
    DCHECK(!ConstraintSpace().IsNewFormattingContext());
    DCHECK(!container_builder_.BfcBlockOffset());

    // If there were preceding adjoining objects, they will be affected when the
    // BFC block-offset gets resolved or updated. We then need to roll back and
    // re-layout those objects with the new BFC block-offset, once the BFC
    // block-offset is updated.
    abort_when_bfc_block_offset_updated_ = true;

    container_builder_.SetAdjoiningObjectTypes(adjoining_object_types);
  }

  if (Style().IsDeprecatedWebkitBoxWithVerticalLineClamp()) {
    is_line_clamp_context_ = true;
    if (!ignore_line_clamp_)
      lines_until_clamp_ = Style().LineClamp();
  } else if (Style().HasLineClamp()) {
    UseCounter::Count(Node().GetDocument(),
                      WebFeature::kWebkitLineClampWithoutWebkitBox);
  }

  LayoutUnit content_edge = BorderScrollbarPadding().block_start;

  NGPreviousInflowPosition previous_inflow_position = {
      LayoutUnit(), ConstraintSpace().MarginStrut(),
      is_resuming_ ? LayoutUnit() : container_builder_.Padding().block_start,
      /* self_collapsing_child_had_clearance */ false};

  // Do not collapse margins between parent and its child if:
  //
  // A: There is border/padding between them.
  // B: This is a new formatting context
  // C: We're resuming layout from a break token. Margin struts cannot pass from
  //    one fragment to another if they are generated by the same block; they
  //    must be dealt with at the first fragment.
  // D: We're forced to stop margin collapsing by a CSS property
  //
  // In all those cases we can and must resolve the BFC block offset now.
  if (content_edge || is_resuming_ ||
      ConstraintSpace().IsNewFormattingContext()) {
    bool discard_subsequent_margins =
        previous_inflow_position.margin_strut.discard_margins && !content_edge;
    if (!ResolveBfcBlockOffset(&previous_inflow_position)) {
      // There should be no preceding content that depends on the BFC block
      // offset of a new formatting context block, and likewise when resuming
      // from a break token.
      DCHECK(!ConstraintSpace().IsNewFormattingContext());
      DCHECK(!is_resuming_);
      return container_builder_.Abort(NGLayoutResult::kBfcBlockOffsetResolved);
    }
    // Move to the content edge. This is where the first child should be placed.
    previous_inflow_position.logical_block_offset = content_edge;

    // If we resolved the BFC block offset now, the margin strut has been
    // reset. If margins are to be discarded, and this box would otherwise have
    // adjoining margins between its own margin and those subsequent content,
    // we need to make sure subsequent content discard theirs.
    if (discard_subsequent_margins)
      previous_inflow_position.margin_strut.discard_margins = true;
  }

#if DCHECK_IS_ON()
  // If this is a new formatting context, we should definitely be at the origin
  // here. If we're resuming from a break token (for a block that doesn't
  // establish a new formatting context), that may not be the case,
  // though. There may e.g. be clearance involved, or inline-start margins.
  if (ConstraintSpace().IsNewFormattingContext())
    DCHECK_EQ(*container_builder_.BfcBlockOffset(), LayoutUnit());
  // If this is a new formatting context, or if we're resuming from a break
  // token, no margin strut must be lingering around at this point.
  if (ConstraintSpace().IsNewFormattingContext() || is_resuming_)
    DCHECK(ConstraintSpace().MarginStrut().IsEmpty());

  if (!container_builder_.BfcBlockOffset()) {
    // New formatting-contexts, and when we have a self-collapsing child
    // affected by clearance must already have their BFC block-offset resolved.
    DCHECK(!previous_inflow_position.self_collapsing_child_had_clearance);
    DCHECK(!ConstraintSpace().IsNewFormattingContext());
  }
#endif

  // If this node is a quirky container, (we are in quirks mode and either a
  // table cell or body), we set our margin strut to a mode where it only
  // considers non-quirky margins. E.g.
  // <body>
  //   <p></p>
  //   <div style="margin-top: 10px"></div>
  //   <h1>Hello</h1>
  // </body>
  // In the above example <p>'s & <h1>'s margins are ignored as they are
  // quirky, and we only consider <div>'s 10px margin.
  if (node_.IsQuirkyContainer())
    previous_inflow_position.margin_strut.is_quirky_container_start = true;

  // Try to reuse line box fragments from cached fragments if possible.
  // When possible, this adds fragments to |container_builder_| and update
  // |previous_inflow_position| and |BreakToken()|.
  scoped_refptr<const NGInlineBreakToken> previous_inline_break_token;

  NGBlockChildIterator child_iterator(Node().FirstChild(), BreakToken());

  // If this layout is blocked by a display-lock, then we pretend this node has
  // no children and that there are no break tokens. Due to this, we skip layout
  // on these children.
  if (Node().ChildLayoutBlockedByDisplayLock())
    child_iterator = NGBlockChildIterator(NGBlockNode(nullptr), nullptr);

  NGBlockNode ruby_text_child(nullptr);
  NGBlockNode placeholder_child(nullptr);
  for (auto entry = child_iterator.NextChild();
       NGLayoutInputNode child = entry.node;
       entry = child_iterator.NextChild(previous_inline_break_token.get())) {
    const NGBreakToken* child_break_token = entry.token;

    if (child.IsOutOfFlowPositioned()) {
      // We don't support fragmentation inside out-of-flow positioned boxes yet,
      // but breaking before is fine. This may happen when a column spanner is
      // directly followed by an OOF.
      DCHECK(!child_break_token ||
             (child_break_token->IsBlockType() &&
              To<NGBlockBreakToken>(child_break_token)->IsBreakBefore()));
      HandleOutOfFlowPositioned(previous_inflow_position,
                                To<NGBlockNode>(child));
    } else if (child.IsFloating()) {
      HandleFloat(previous_inflow_position, To<NGBlockNode>(child),
                  To<NGBlockBreakToken>(child_break_token));
    } else if (child.IsListMarker() && !child.ListMarkerOccupiesWholeLine()) {
      container_builder_.SetUnpositionedListMarker(
          NGUnpositionedListMarker(To<NGBlockNode>(child)));
    } else if (child.IsColumnSpanAll() && ConstraintSpace().IsInColumnBfc()) {
      // The child is a column spanner. We now need to finish this
      // fragmentainer, then abort and let the column layout algorithm handle
      // the spanner as a child.
      DCHECK(!container_builder_.DidBreakSelf());
      DCHECK(!container_builder_.FoundColumnSpanner());
      DCHECK(!child_break_token);
      container_builder_.SetColumnSpanner(To<NGBlockNode>(child));
      // After the spanner(s), we are going to resume inside this block. If
      // there's a subsequent sibling that's not a spanner, we're resume right
      // in front of that one. Otherwise we'll just resume after all the
      // children.
      for (entry = child_iterator.NextChild();
           NGLayoutInputNode sibling = entry.node;
           entry = child_iterator.NextChild()) {
        DCHECK(!entry.token);
        if (sibling.IsColumnSpanAll())
          continue;
        container_builder_.AddBreakBeforeChild(sibling, kBreakAppealPerfect,
                                               /* is_forced_break */ true);
        break;
      }
      break;
    } else if (IsRubyText(child)) {
      ruby_text_child = To<NGBlockNode>(child);
    } else if (child.IsTextControlPlaceholder()) {
      placeholder_child = To<NGBlockNode>(child);
    } else {
      // If this is the child we had previously determined to break before, do
      // so now and finish layout.
      if (UNLIKELY(
              early_break_ &&
              IsEarlyBreakpoint(*early_break_, container_builder_, child))) {
        if (!ResolveBfcBlockOffset(&previous_inflow_position)) {
          // However, the predetermined breakpoint may be exactly where the BFC
          // block-offset gets resolved. If that hasn't yet happened, we need to
          // do that first and re-layout at the right BFC block-offset, and THEN
          // break.
          return container_builder_.Abort(
              NGLayoutResult::kBfcBlockOffsetResolved);
        }
        container_builder_.AddBreakBeforeChild(child, kBreakAppealPerfect,
                                               /* is_forced_break */ false);
        ConsumeRemainingFragmentainerSpace(&previous_inflow_position);
        break;
      }

      NGLayoutResult::EStatus status;
      if (child.CreatesNewFormattingContext()) {
        status = HandleNewFormattingContext(child, child_break_token,
                                            &previous_inflow_position);
        previous_inline_break_token = nullptr;
      } else {
        status = HandleInflow(
            child, child_break_token, &previous_inflow_position,
            inline_child_layout_context, &previous_inline_break_token);
      }

      if (status != NGLayoutResult::kSuccess) {
        // We need to abort the layout. No fragment will be generated.
        return container_builder_.Abort(status);
      }
      if (ConstraintSpace().HasBlockFragmentation()) {
        // A child break in a parallel flow doesn't affect whether we should
        // break here or not.
        if (container_builder_.HasInflowChildBreakInside()) {
          // But if the break happened in the same flow, we'll now just finish
          // layout of the fragment. No more siblings should be processed.
          break;
        }

        // We need to propagate the initial break-before value up our container
        // chain, until we reach a container that's not a first child. If we get
        // all the way to the root of the fragmentation context without finding
        // any such container, we have no valid class A break point, and if a
        // forced break was requested, none will be inserted.
        if (!has_processed_first_child_ && !child.IsInline())
          container_builder_.SetInitialBreakBefore(child.Style().BreakBefore());
        has_processed_first_child_ = true;
      }
    }
  }

  if (ruby_text_child)
    HandleRubyText(ruby_text_child);
  if (placeholder_child)
    HandleTextControlPlaceholder(placeholder_child, previous_inflow_position);

  if (UNLIKELY(ConstraintSpace().IsNewFormattingContext() &&
               !ignore_line_clamp_ && lines_until_clamp_ == 0 &&
               intrinsic_block_size_when_clamped_)) {
    // Truncation of the last line was forced, but there are no lines after the
    // truncated line. Rerun layout without forcing truncation. This is only
    // done if line-clamp was specified on the element as the element containing
    // the node may have subsequent lines. If there aren't, the containing
    // element will relayout.
    return container_builder_.Abort(
        NGLayoutResult::kNeedsRelayoutWithNoForcedTruncateAtLineClamp);
  }

  if (!child_iterator.NextChild(previous_inline_break_token.get()).node) {
    // We've gone through all the children. This doesn't necessarily mean that
    // we're done fragmenting, as there may be parallel flows [1] (visible
    // overflow) still needing more space than what the current fragmentainer
    // can provide. It does mean, though, that, for any future fragmentainers,
    // we'll just be looking at the break tokens, if any, and *not* start laying
    // out any nodes from scratch, since we have started/finished all the
    // children, or at least created break tokens for them.
    //
    // [1] https://drafts.csswg.org/css-break/#parallel-flows
    container_builder_.SetHasSeenAllChildren();
  }

  // The intrinsic block size is not allowed to be less than the content edge
  // offset, as that could give us a negative content box size.
  intrinsic_block_size_ = content_edge;

  // To save space of the stack when we recurse into children, the rest of this
  // function is continued within |FinishLayout|. However it should be read as
  // one function.
  return FinishLayout(&previous_inflow_position, inline_child_layout_context);
}

scoped_refptr<const NGLayoutResult> NGBlockLayoutAlgorithm::FinishLayout(
    NGPreviousInflowPosition* previous_inflow_position,
    NGInlineChildLayoutContext* inline_child_layout_context) {
  LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();
  NGMarginStrut end_margin_strut = previous_inflow_position->margin_strut;

  // Add line height for empty content editable or button with empty label, e.g.
  // <div contenteditable></div>, <input type="button" value="">
  if (container_builder_.HasSeenAllChildren() &&
      HasLineEvenIfEmpty(Node().GetLayoutBox())) {
    intrinsic_block_size_ +=
        std::max(intrinsic_block_size_,
                 Node().GetLayoutBox()->LogicalHeightForEmptyLine());
    // Test [1][2] require baseline offset for empty editable.
    // [1] css3/flexbox/baseline-for-empty-line.html
    // [2] inline-block/contenteditable-baseline.html
    const LayoutBlock* const layout_block =
        To<LayoutBlock>(Node().GetLayoutBox());
    if (auto baseline_offset = layout_block->BaselineForEmptyLine(
            layout_block->IsHorizontalWritingMode() ? kHorizontalLine
                                                    : kVerticalLine))
      container_builder_.SetBaseline(*baseline_offset);
  }

  // Collapse annotation overflow and padding.
  // logical_block_offset already contains block-end annotation overflow.
  // However, if the container has non-zero block-end padding, the annotation
  // can extend on the padding. So we decrease logical_block_offset by
  // shareable part of the annotation overflow and the padding.
  if (previous_inflow_position->block_end_annotation_space < LayoutUnit()) {
    const LayoutUnit annotation_overflow =
        -previous_inflow_position->block_end_annotation_space;
    previous_inflow_position->logical_block_offset -=
        std::min(container_builder_.Padding().block_end, annotation_overflow);
  }

  // If the current layout is a new formatting context, we need to encapsulate
  // all of our floats.
  if (ConstraintSpace().IsNewFormattingContext()) {
    intrinsic_block_size_ = std::max(
        intrinsic_block_size_, exclusion_space_.ClearanceOffset(EClear::kBoth));
  }

  // If line clamping occurred, the intrinsic block-size comes from the
  // intrinsic block-size at the time of the clamp.
  if (intrinsic_block_size_when_clamped_) {
    DCHECK(container_builder_.BfcBlockOffset());
    intrinsic_block_size_ = *intrinsic_block_size_when_clamped_ +
                            BorderScrollbarPadding().block_end;
    end_margin_strut = NGMarginStrut();
  } else if (BorderScrollbarPadding().block_end ||
             previous_inflow_position->self_collapsing_child_had_clearance ||
             ConstraintSpace().IsNewFormattingContext()) {
    // The end margin strut of an in-flow fragment contributes to the size of
    // the current fragment if:
    //  - There is block-end border/scrollbar/padding.
    //  - There was a self-collapsing child affected by clearance.
    //  - We are a new formatting context.
    // Additionally this fragment produces no end margin strut.

    if (!container_builder_.BfcBlockOffset()) {
      // If we have collapsed through the block start and all children (if any),
      // now is the time to determine the BFC block offset, because finally we
      // have found something solid to hang on to (like clearance or a bottom
      // border, for instance). If we're a new formatting context, though, we
      // shouldn't be here, because then the offset should already have been
      // determined.
      DCHECK(!ConstraintSpace().IsNewFormattingContext());
      if (!ResolveBfcBlockOffset(previous_inflow_position)) {
        return container_builder_.Abort(
            NGLayoutResult::kBfcBlockOffsetResolved);
      }
      DCHECK(container_builder_.BfcBlockOffset());
    } else {
      // If we are a quirky container, we ignore any quirky margins and just
      // consider normal margins to extend our size.  Other UAs perform this
      // calculation differently, e.g. by just ignoring the *last* quirky
      // margin.
      LayoutUnit margin_strut_sum = node_.IsQuirkyContainer()
                                        ? end_margin_strut.QuirkyContainerSum()
                                        : end_margin_strut.Sum();

      if (ConstraintSpace().HasKnownFragmentainerBlockSize()) {
        LayoutUnit bfc_block_offset =
            *container_builder_.BfcBlockOffset() +
            previous_inflow_position->logical_block_offset;
        margin_strut_sum = AdjustedMarginAfterFinalChildFragment(
            ConstraintSpace(), bfc_block_offset, margin_strut_sum);
      }

      // The trailing margin strut will be part of our intrinsic block size, but
      // only if there is something that separates the end margin strut from the
      // input margin strut (typically child content, block start
      // border/padding, or this being a new BFC). If the margin strut from a
      // previous sibling or ancestor managed to collapse through all our
      // children (if any at all, that is), it means that the resulting end
      // margin strut actually pushes us down, and it should obviously not be
      // doubly accounted for as our block size.
      intrinsic_block_size_ = std::max(
          intrinsic_block_size_,
          previous_inflow_position->logical_block_offset + margin_strut_sum);
    }

    intrinsic_block_size_ += BorderScrollbarPadding().block_end;
    end_margin_strut = NGMarginStrut();
  } else {
    // Update our intrinsic block size to be just past the block-end border edge
    // of the last in-flow child. The pending margin is to be propagated to our
    // container, so ignore it.
    intrinsic_block_size_ = std::max(
        intrinsic_block_size_, previous_inflow_position->logical_block_offset);
  }

  // Save the unconstrained intrinsic size on the builder before clamping it.
  LayoutUnit unconstrained_intrinsic_block_size = intrinsic_block_size_;
  container_builder_.SetOverflowBlockSize(intrinsic_block_size_);

  intrinsic_block_size_ = ClampIntrinsicBlockSize(
      ConstraintSpace(), Node(), BorderScrollbarPadding(),
      intrinsic_block_size_,
      CalculateQuirkyBodyMarginBlockSum(end_margin_strut));

  LayoutUnit previously_consumed_block_size;
  if (UNLIKELY(BreakToken()))
    previously_consumed_block_size = BreakToken()->ConsumedBlockSize();

  // Recompute the block-axis size now that we know our content size.
  border_box_size.block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(),
      previously_consumed_block_size + intrinsic_block_size_,
      border_box_size.inline_size);
  container_builder_.SetFragmentsTotalBlockSize(border_box_size.block_size);

  // If our BFC block-offset is still unknown, we check:
  //  - If we have a non-zero block-size (margins don't collapse through us).
  //  - If we have a break token. (Even if we are self-collapsing we position
  //    ourselves at the very start of the fragmentainer).
  //  - We got interrupted by a column spanner.
  if (!container_builder_.BfcBlockOffset() &&
      (border_box_size.block_size || BreakToken() ||
       container_builder_.FoundColumnSpanner())) {
    if (!ResolveBfcBlockOffset(previous_inflow_position))
      return container_builder_.Abort(NGLayoutResult::kBfcBlockOffsetResolved);
    DCHECK(container_builder_.BfcBlockOffset());
  }

  if (container_builder_.BfcBlockOffset()) {
    // Do not collapse margins between the last in-flow child and bottom margin
    // of its parent if:
    //  - The block-size differs from the intrinsic size.
    //  - The parent has computed block-size != auto.
    if (border_box_size.block_size != intrinsic_block_size_ ||
        !BlockLengthUnresolvable(ConstraintSpace(), Style().LogicalHeight(),
                                 LengthResolvePhase::kLayout)) {
      end_margin_strut = NGMarginStrut();
    }
  }

  // List markers should have been positioned if we had line boxes, or boxes
  // that have line boxes. If there were no line boxes, position without line
  // boxes.
  if (container_builder_.UnpositionedListMarker() && node_.IsListItem()) {
    if (!PositionListMarkerWithoutLineBoxes(previous_inflow_position))
      return container_builder_.Abort(NGLayoutResult::kBfcBlockOffsetResolved);
  }

  container_builder_.SetEndMarginStrut(end_margin_strut);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);

  if (container_builder_.BfcBlockOffset()) {
    // If we know our BFC block-offset we should have correctly placed all
    // adjoining objects, and shouldn't propagate this information to siblings.
    container_builder_.ResetAdjoiningObjectTypes();
  } else {
    // If we don't know our BFC block-offset yet, we know that for
    // margin-collapsing purposes we are self-collapsing.
    container_builder_.SetIsSelfCollapsing();

    // If we've been forced at a particular BFC block-offset, (either from
    // clearance past adjoining floats, or a re-layout), we can safely set our
    // BFC block-offset now.
    if (ConstraintSpace().ForcedBfcBlockOffset()) {
      container_builder_.SetBfcBlockOffset(
          *ConstraintSpace().ForcedBfcBlockOffset());
    }
  }

  // At this point, perform any final table-cell adjustments needed.
  if (ConstraintSpace().IsTableCell())
    FinalizeForTableCell(unconstrained_intrinsic_block_size);

  // We only finalize for fragmentation if the fragment has a BFC block offset.
  // This may occur with a zero block size fragment. We need to know the BFC
  // block offset to determine where the fragmentation line is relative to us.
  if (container_builder_.BfcBlockOffset() &&
      ConstraintSpace().HasBlockFragmentation()) {
    if (!FinalizeForFragmentation())
      return container_builder_.Abort(NGLayoutResult::kNeedsEarlierBreak);
  }

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

#if DCHECK_IS_ON()
  // If we're not participating in a fragmentation context, no block
  // fragmentation related fields should have been set.
  if (!ConstraintSpace().HasBlockFragmentation())
    container_builder_.CheckNoBlockFragmentation();
#endif

  // Adjust the position of the final baseline if needed.
  container_builder_.SetLastBaselineToBlockEndMarginEdgeIfNeeded();

  // An exclusion space is confined to nodes within the same formatting context.
  if (!ConstraintSpace().IsNewFormattingContext()) {
    container_builder_.SetExclusionSpace(std::move(exclusion_space_));
    container_builder_.SetLinesUntilClamp(lines_until_clamp_);
  }

  if (ConstraintSpace().UseFirstLineStyle())
    container_builder_.SetStyleVariant(NGStyleVariant::kFirstLine);

  return container_builder_.ToBoxFragment();
}

bool NGBlockLayoutAlgorithm::TryReuseFragmentsFromCache(
    NGInlineNode inline_node,
    NGPreviousInflowPosition* previous_inflow_position,
    scoped_refptr<const NGInlineBreakToken>* inline_break_token_out) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  DCHECK(previous_result_);
  DCHECK(!inline_node.IsEmptyInline());
  DCHECK(container_builder_.BfcBlockOffset());
  DCHECK(previous_inflow_position->margin_strut.IsEmpty());
  DCHECK(!previous_inflow_position->self_collapsing_child_had_clearance);

  const auto& previous_fragment =
      To<NGPhysicalBoxFragment>(previous_result_->PhysicalFragment());
  const NGFragmentItems* previous_items = previous_fragment.Items();
  DCHECK(previous_items);

  // Find reusable lines. Fail if no items are reusable.
  // TODO(kojii): |DirtyLinesFromNeedsLayout| is needed only once for a
  // |LayoutBlockFlow|, not for every fragment.
  NGFragmentItems::DirtyLinesFromNeedsLayout(*inline_node.GetLayoutBlockFlow());
  const NGFragmentItem* end_item =
      previous_items->EndOfReusableItems(previous_fragment);
  DCHECK(end_item);
  if (!end_item || end_item == &previous_items->front())
    return false;

  wtf_size_t max_lines = 0;
  if (lines_until_clamp_) {
    // There is an additional logic for the last clamped line. Reuse only up to
    // before that to use the same logic.
    if (*lines_until_clamp_ <= 1)
      return false;
    max_lines = *lines_until_clamp_ - 1;
  }

  const auto& children = container_builder_.Children();
  const wtf_size_t children_before = children.size();
  NGFragmentItemsBuilder* items_builder = container_builder_.ItemsBuilder();
  const NGConstraintSpace& space = ConstraintSpace();
  DCHECK_EQ(items_builder->GetWritingDirection(), space.GetWritingDirection());
  const auto result =
      items_builder->AddPreviousItems(previous_fragment, *previous_items,
                                      &container_builder_, end_item, max_lines);
  if (UNLIKELY(!result.succeeded)) {
    DCHECK_EQ(children.size(), children_before);
    DCHECK(!result.used_block_size);
    DCHECK(!result.inline_break_token);
    return false;
  }

  DCHECK_GT(result.line_count, 0u);
  DCHECK(!max_lines || result.line_count <= max_lines);
  if (lines_until_clamp_) {
    DCHECK_GT(*lines_until_clamp_, static_cast<int>(result.line_count));
    lines_until_clamp_ = *lines_until_clamp_ - result.line_count;
  }

  // |AddPreviousItems| may have added more than one lines. Propagate baselines
  // from them.
  for (const auto& child : base::make_span(children).subspan(children_before)) {
    DCHECK(child.fragment->IsLineBox());
    PropagateBaselineFromChild(To<NGPhysicalContainerFragment>(*child.fragment),
                               child.offset.block_offset);
  }

  previous_inflow_position->logical_block_offset += result.used_block_size;
  *inline_break_token_out = result.inline_break_token;
  return true;
}

void NGBlockLayoutAlgorithm::HandleOutOfFlowPositioned(
    const NGPreviousInflowPosition& previous_inflow_position,
    NGBlockNode child) {
  DCHECK(child.IsOutOfFlowPositioned());
  LogicalOffset static_offset = {BorderScrollbarPadding().inline_start,
                                 previous_inflow_position.logical_block_offset};

  // We only include the margin strut in the OOF static-position if we know we
  // aren't going to be a zero-block-size fragment.
  if (container_builder_.BfcBlockOffset())
    static_offset.block_offset += previous_inflow_position.margin_strut.Sum();

  if (child.Style().IsOriginalDisplayInlineType()) {
    // The static-position of inline-level OOF-positioned nodes depends on
    // previous floats (if any).
    //
    // Due to this we need to mark this node as having adjoining objects, and
    // perform a re-layout if our position shifts.
    if (!container_builder_.BfcBlockOffset()) {
      container_builder_.AddAdjoiningObjectTypes(kAdjoiningInlineOutOfFlow);
      abort_when_bfc_block_offset_updated_ = true;
    }

    LayoutUnit origin_bfc_block_offset =
        container_builder_.BfcBlockOffset().value_or(
            ConstraintSpace().ExpectedBfcBlockOffset()) +
        static_offset.block_offset;

    NGBfcOffset origin_bfc_offset = {
        ConstraintSpace().BfcOffset().line_offset +
            BorderScrollbarPadding().LineLeft(Style().Direction()),
        origin_bfc_block_offset};

    static_offset.inline_offset += CalculateOutOfFlowStaticInlineLevelOffset(
        Style(), origin_bfc_offset, exclusion_space_,
        ChildAvailableSize().inline_size);
  }

  container_builder_.AddOutOfFlowChildCandidate(child, static_offset);
}

void NGBlockLayoutAlgorithm::HandleFloat(
    const NGPreviousInflowPosition& previous_inflow_position,
    NGBlockNode child,
    const NGBlockBreakToken* child_break_token) {
  // If we're resuming layout, we must always know our position in the BFC.
  DCHECK(!IsResumingLayout(child_break_token) ||
         container_builder_.BfcBlockOffset());

  // If we don't have a BFC block-offset yet, the "expected" BFC block-offset
  // is used to optimistically place floats.
  NGBfcOffset origin_bfc_offset = {
      ConstraintSpace().BfcOffset().line_offset +
          BorderScrollbarPadding().LineLeft(ConstraintSpace().Direction()),
      container_builder_.BfcBlockOffset()
          ? NextBorderEdge(previous_inflow_position)
          : ConstraintSpace().ExpectedBfcBlockOffset()};

  if (ConstraintSpace().HasBlockFragmentation()) {
    // Forced breaks cannot be specified directly on floats, but if the
    // preceding block has a forced break after, we need to break before this
    // float.
    EBreakBetween break_between =
        container_builder_.JoinedBreakBetweenValue(EBreakBetween::kAuto);
    if (IsForcedBreakValue(ConstraintSpace(), break_between)) {
      container_builder_.AddBreakBeforeChild(child, kBreakAppealPerfect,
                                             /* is_forced_break*/ true);
      return;
    }
  }

  NGUnpositionedFloat unpositioned_float(
      child, child_break_token, ChildAvailableSize(), child_percentage_size_,
      replaced_child_percentage_size_, origin_bfc_offset, ConstraintSpace(),
      Style());

  if (!container_builder_.BfcBlockOffset()) {
    container_builder_.AddAdjoiningObjectTypes(
        unpositioned_float.IsLineLeft(ConstraintSpace().Direction())
            ? kAdjoiningFloatLeft
            : kAdjoiningFloatRight);
    // If we don't have a forced BFC block-offset yet, we'll optimistically
    // place floats at the "expected" BFC block-offset. If this differs from
    // our final BFC block-offset we'll need to re-layout.
    if (!ConstraintSpace().ForcedBfcBlockOffset())
      abort_when_bfc_block_offset_updated_ = true;
  }

  NGPositionedFloat positioned_float =
      PositionFloat(&unpositioned_float, &exclusion_space_);

  const NGLayoutResult& layout_result = *positioned_float.layout_result;

  // TODO(mstensho): Handle abortions caused by block fragmentation.
  DCHECK_EQ(layout_result.Status(), NGLayoutResult::kSuccess);

  if (positioned_float.need_break_before) {
    DCHECK(ConstraintSpace().HasBlockFragmentation());
    LayoutUnit fragmentainer_block_offset =
        ConstraintSpace().FragmentainerOffsetAtBfc() +
        positioned_float.bfc_offset.block_offset;
    BreakBeforeChild(ConstraintSpace(), child, *positioned_float.layout_result,
                     fragmentainer_block_offset,
                     /* appeal */ base::nullopt,
                     /* is_forced_break */ false, &container_builder_);

    // After breaking before the float, carry on with layout of this
    // container. The float constitutes a parallel flow, and there may be
    // siblings that could still fit in the current fragmentainer.
    return;
  }

  // TODO(mstensho): There should be a class A breakpoint between a float and
  // another float, and also between a float and an in-flow block.

  const NGPhysicalFragment& physical_fragment =
      positioned_float.layout_result->PhysicalFragment();
  LayoutUnit float_inline_size =
      NGFragment(ConstraintSpace().GetWritingDirection(), physical_fragment)
          .InlineSize();

  NGBfcOffset bfc_offset = {ConstraintSpace().BfcOffset().line_offset,
                            container_builder_.BfcBlockOffset().value_or(
                                ConstraintSpace().ExpectedBfcBlockOffset())};

  LogicalOffset logical_offset = LogicalFromBfcOffsets(
      positioned_float.bfc_offset, bfc_offset, float_inline_size,
      container_builder_.Size().inline_size, ConstraintSpace().Direction());

  container_builder_.AddResult(*positioned_float.layout_result, logical_offset);
}

NGLayoutResult::EStatus NGBlockLayoutAlgorithm::HandleNewFormattingContext(
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token,
    NGPreviousInflowPosition* previous_inflow_position) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK(!child.IsOutOfFlowPositioned());
  DCHECK(child.CreatesNewFormattingContext());
  DCHECK(child.IsBlock());

  const ComputedStyle& child_style = child.Style();
  const TextDirection direction = ConstraintSpace().Direction();
  NGInflowChildData child_data =
      ComputeChildData(*previous_inflow_position, child, child_break_token,
                       /* is_new_fc */ true);

  LayoutUnit child_origin_line_offset =
      ConstraintSpace().BfcOffset().line_offset +
      BorderScrollbarPadding().LineLeft(direction);

  // If the child has a block-start margin, and the BFC block offset is still
  // unresolved, and we have preceding adjoining floats, things get complicated
  // here. Depending on whether the child fits beside the floats, the margin may
  // or may not be adjoining with the current margin strut. This affects the
  // position of the preceding adjoining floats. We may have to resolve the BFC
  // block offset once with the child's margin tentatively adjoining, then
  // realize that the child isn't going to fit beside the floats at the current
  // position, and therefore re-resolve the BFC block offset with the child's
  // margin non-adjoining. This is akin to clearance.
  NGMarginStrut adjoining_margin_strut(previous_inflow_position->margin_strut);
  adjoining_margin_strut.Append(child_data.margins.block_start,
                                child_style.HasMarginBeforeQuirk());
  LayoutUnit adjoining_bfc_offset_estimate =
      child_data.bfc_offset_estimate.block_offset +
      adjoining_margin_strut.Sum();
  LayoutUnit non_adjoining_bfc_offset_estimate =
      child_data.bfc_offset_estimate.block_offset +
      previous_inflow_position->margin_strut.Sum();
  LayoutUnit child_bfc_offset_estimate = adjoining_bfc_offset_estimate;
  bool bfc_offset_already_resolved = false;
  bool child_determined_bfc_offset = false;
  bool child_margin_got_separated = false;
  bool has_adjoining_floats = false;

  if (!container_builder_.BfcBlockOffset()) {
    has_adjoining_floats =
        container_builder_.AdjoiningObjectTypes() & kAdjoiningFloatBoth;

    // If this node, or an arbitrary ancestor had clearance past adjoining
    // floats, we consider the margin "separated". We should *never* attempt to
    // re-resolve the BFC block-offset in this case.
    bool has_clearance_past_adjoining_floats =
        ConstraintSpace().AncestorHasClearancePastAdjoiningFloats() ||
        HasClearancePastAdjoiningFloats(
            container_builder_.AdjoiningObjectTypes(), child_style, Style());

    if (has_clearance_past_adjoining_floats) {
      child_bfc_offset_estimate = NextBorderEdge(*previous_inflow_position);
      child_margin_got_separated = true;
    } else if (ConstraintSpace().ForcedBfcBlockOffset()) {
      // This is not the first time we're here. We already have a suggested BFC
      // block offset.
      bfc_offset_already_resolved = true;
      child_bfc_offset_estimate = *ConstraintSpace().ForcedBfcBlockOffset();
      // We require that the BFC block offset be the one we'd get with margins
      // adjoining, margins separated, or if clearance was applied to either of
      // these. Anything else is a bug.
      DCHECK(child_bfc_offset_estimate == adjoining_bfc_offset_estimate ||
             child_bfc_offset_estimate == non_adjoining_bfc_offset_estimate ||
             child_bfc_offset_estimate == ConstraintSpace().ClearanceOffset());
      // Figure out if the child margin has already got separated from the
      // margin strut or not.
      child_margin_got_separated =
          child_bfc_offset_estimate != adjoining_bfc_offset_estimate;
    }

    // The BFC block offset of this container gets resolved because of this
    // child.
    child_determined_bfc_offset = true;

    // The block-start margin of the child will only affect the parent's
    // position if it is adjoining.
    if (!child_margin_got_separated) {
      SetSubtreeModifiedMarginStrutIfNeeded(
          &child_style.MarginBeforeUsing(Style()));
    }

    if (!ResolveBfcBlockOffset(previous_inflow_position,
                               child_bfc_offset_estimate)) {
      // If we need to abort here, it means that we had preceding unpositioned
      // floats. This is only expected if we're here for the first time.
      DCHECK(!bfc_offset_already_resolved);
      return NGLayoutResult::kBfcBlockOffsetResolved;
    }

    // We reset the block offset here as it may have been affected by clearance.
    child_bfc_offset_estimate = ContainerBfcOffset().block_offset;
  }

  // If the child has a non-zero block-start margin, our initial estimate will
  // be that any pending floats will be flush (block-start-wise) with this
  // child, since they are affected by margin collapsing. Furthermore, this
  // child's margin may also pull parent blocks downwards. However, this is only
  // the case if the child fits beside the floats at the current block
  // offset. If it doesn't (or if it gets clearance), the child needs to be
  // pushed down. In this case, the child's margin no longer collapses with the
  // previous margin strut, so the pending floats and parent blocks need to
  // ignore this margin, which may cause them to end up at completely different
  // positions than initially estimated. In other words, we'll need another
  // layout pass if this happens.
  bool abort_if_cleared = child_data.margins.block_start != LayoutUnit() &&
                          !child_margin_got_separated &&
                          child_determined_bfc_offset;
  NGBfcOffset child_bfc_offset;
  scoped_refptr<const NGLayoutResult> layout_result =
      LayoutNewFormattingContext(
          child, child_break_token, child_data,
          {child_origin_line_offset, child_bfc_offset_estimate},
          abort_if_cleared, &child_bfc_offset);

  if (!layout_result) {
    DCHECK(abort_if_cleared);
    // Layout got aborted, because the child got pushed down by floats, and we
    // may have had pending floats that we tentatively positioned incorrectly
    // (since the child's margin shouldn't have affected them). Try again
    // without the child's margin. So, we need another layout pass. Figure out
    // if we can do it right away from here, or if we have to roll back and
    // reposition floats first.
    if (child_determined_bfc_offset) {
      // The BFC block offset was calculated when we got to this child, with
      // the child's margin adjoining. Since that turned out to be wrong,
      // re-resolve the BFC block offset without the child's margin.
      LayoutUnit old_offset = *container_builder_.BfcBlockOffset();
      container_builder_.ResetBfcBlockOffset();

      // Re-resolving the BFC block-offset with a different "forced" BFC
      // block-offset is only safe if an ancestor *never* had clearance past
      // adjoining floats.
      DCHECK(!ConstraintSpace().AncestorHasClearancePastAdjoiningFloats());
      ResolveBfcBlockOffset(previous_inflow_position,
                            non_adjoining_bfc_offset_estimate,
                            /* forced_bfc_block_offset */ base::nullopt);

      if ((bfc_offset_already_resolved || has_adjoining_floats) &&
          old_offset != *container_builder_.BfcBlockOffset()) {
        // The first BFC block offset resolution turned out to be wrong, and we
        // positioned preceding adjacent floats based on that. Now we have to
        // roll back and position them at the correct offset. The only expected
        // incorrect estimate is with the child's margin adjoining. Any other
        // incorrect estimate will result in failed layout.
        DCHECK_EQ(old_offset, adjoining_bfc_offset_estimate);
        return NGLayoutResult::kBfcBlockOffsetResolved;
      }
    }

    child_bfc_offset_estimate = non_adjoining_bfc_offset_estimate;
    child_margin_got_separated = true;

    // We can re-layout the child right away. This re-layout *must* produce a
    // fragment which fits within the exclusion space.
    layout_result = LayoutNewFormattingContext(
        child, child_break_token, child_data,
        {child_origin_line_offset, child_bfc_offset_estimate},
        /* abort_if_cleared */ false, &child_bfc_offset);
  }

  if (ConstraintSpace().HasBlockFragmentation()) {
    bool has_container_separation =
        has_processed_first_child_ || child_margin_got_separated ||
        child_bfc_offset.block_offset > child_bfc_offset_estimate ||
        layout_result->IsPushedByFloats();
    NGBreakStatus break_status = BreakBeforeChildIfNeeded(
        child, *layout_result, previous_inflow_position,
        child_bfc_offset.block_offset, has_container_separation);
    if (break_status == NGBreakStatus::kBrokeBefore)
      return NGLayoutResult::kSuccess;
    if (break_status == NGBreakStatus::kNeedsEarlierBreak)
      return NGLayoutResult::kNeedsEarlierBreak;

    // If the child aborted layout, we cannot continue.
    DCHECK_EQ(layout_result->Status(), NGLayoutResult::kSuccess);

    EBreakBetween break_after = JoinFragmentainerBreakValues(
        layout_result->FinalBreakAfter(), child.Style().BreakAfter());
    container_builder_.SetPreviousBreakAfter(break_after);
  }

  const auto& physical_fragment = layout_result->PhysicalFragment();
  NGFragment fragment(ConstraintSpace().GetWritingDirection(),
                      physical_fragment);

  LogicalOffset logical_offset = LogicalFromBfcOffsets(
      child_bfc_offset, ContainerBfcOffset(), fragment.InlineSize(),
      container_builder_.Size().inline_size, ConstraintSpace().Direction());

  if (!PositionOrPropagateListMarker(*layout_result, &logical_offset,
                                     previous_inflow_position))
    return NGLayoutResult::kBfcBlockOffsetResolved;

  if (UNLIKELY(child.Style().AlignSelfBlockCenter())) {
    DCHECK(Node().IsTextField());
    // The block-size of a textfield doesn't depend on its contents, so we can
    // compute the block-size without passing the actual intrinsic block-size.
    const LayoutUnit bsp_block_sum = BorderScrollbarPadding().BlockSum();
    LayoutUnit block_size = ClampIntrinsicBlockSize(
        ConstraintSpace(), Node(), BorderScrollbarPadding(), bsp_block_sum);
    block_size = ComputeBlockSizeForFragment(
        ConstraintSpace(), Style(), BorderPadding(), block_size,
        container_builder_.InitialBorderBoxSize().inline_size);
    block_size -= bsp_block_sum;
    logical_offset =
        CenterBlockChild(logical_offset, block_size, fragment.BlockSize());
  }

  PropagateBaselineFromChild(physical_fragment, logical_offset.block_offset);
  container_builder_.AddResult(*layout_result, logical_offset);

  // The margins we store will be used by e.g. getComputedStyle().
  // When calculating these values, ignore any floats that might have
  // affected the child. This is what Edge does.
  ResolveInlineMargins(child_style, Style(), ChildAvailableSize().inline_size,
                       fragment.InlineSize(), &child_data.margins);
  To<NGBlockNode>(child).StoreMargins(ConstraintSpace(), child_data.margins);

  *previous_inflow_position = ComputeInflowPosition(
      *previous_inflow_position, child, child_data,
      child_bfc_offset.block_offset, logical_offset, *layout_result, fragment,
      /* self_collapsing_child_had_clearance */ false);

  return NGLayoutResult::kSuccess;
}

scoped_refptr<const NGLayoutResult>
NGBlockLayoutAlgorithm::LayoutNewFormattingContext(
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token,
    const NGInflowChildData& child_data,
    NGBfcOffset origin_offset,
    bool abort_if_cleared,
    NGBfcOffset* out_child_bfc_offset) {
  const ComputedStyle& child_style = child.Style();
  const TextDirection direction = ConstraintSpace().Direction();
  const auto writing_direction = ConstraintSpace().GetWritingDirection();

  // The origin offset is where we should start looking for layout
  // opportunities. It needs to be adjusted by the child's clearance.
  AdjustToClearance(
      exclusion_space_.ClearanceOffset(child_style.Clear(Style())),
      &origin_offset);
  DCHECK(container_builder_.BfcBlockOffset());

  LayoutOpportunityVector opportunities =
      exclusion_space_.AllLayoutOpportunities(origin_offset,
                                              ChildAvailableSize().inline_size);

  // We should always have at least one opportunity.
  DCHECK_GT(opportunities.size(), 0u);

  // Now we lay out. This will give us a child fragment and thus its size, which
  // means that we can find out if it's actually going to fit. If it doesn't
  // fit where it was laid out, and is pushed downwards, we'll lay out over
  // again, since a new BFC block offset could result in a new fragment size,
  // e.g. when inline size is auto, or if we're block-fragmented.
  for (const auto& opportunity : opportunities) {
    if (abort_if_cleared &&
        origin_offset.block_offset < opportunity.rect.BlockStartOffset()) {
      // Abort if we got pushed downwards. We need to adjust
      // origin_offset.block_offset, reposition any floats affected by that, and
      // try again.
      return nullptr;
    }

    // Find the available inline-size which should be given to the child.
    LayoutUnit line_left_offset = opportunity.rect.start_offset.line_offset;
    LayoutUnit line_right_offset = opportunity.rect.end_offset.line_offset;

    LayoutUnit line_left_margin = child_data.margins.LineLeft(direction);
    LayoutUnit line_right_margin = child_data.margins.LineRight(direction);

    // When the inline dimensions of layout opportunity match the available
    // inline-size, a new formatting context can expand outside of the
    // opportunity if negative margins are present.
    bool can_expand_outside_opportunity =
        opportunity.rect.start_offset.line_offset ==
            origin_offset.line_offset &&
        opportunity.rect.InlineSize() == ChildAvailableSize().inline_size;

    if (can_expand_outside_opportunity) {
      // No floats have affected the available inline-size, adjust the
      // available inline-size by the margins.
      DCHECK_EQ(line_left_offset, origin_offset.line_offset);
      DCHECK_EQ(line_right_offset,
                origin_offset.line_offset + ChildAvailableSize().inline_size);
      line_left_offset += line_left_margin;
      line_right_offset -= line_right_margin;
    } else {
      // Margins are applied from the content-box, not the layout opportunity
      // area. Instead of adjusting by the size of the margins, we "shrink" the
      // available inline-size if required.
      line_left_offset = std::max(
          line_left_offset,
          origin_offset.line_offset + line_left_margin.ClampNegativeToZero());
      line_right_offset = std::min(line_right_offset,
                                   origin_offset.line_offset +
                                       ChildAvailableSize().inline_size -
                                       line_right_margin.ClampNegativeToZero());
    }
    LayoutUnit opportunity_size =
        (line_right_offset - line_left_offset).ClampNegativeToZero();

    // The available inline size in the child constraint space needs to include
    // inline margins, since layout algorithms (both legacy and NG) will resolve
    // auto inline size by subtracting the inline margins from available inline
    // size. We have calculated a layout opportunity without margins in mind,
    // since they overlap with adjacent floats. Now we need to add them.
    LayoutUnit child_available_inline_size =
        (opportunity_size + child_data.margins.InlineSum())
            .ClampNegativeToZero();

    NGConstraintSpace child_space = CreateConstraintSpaceForChild(
        child, child_data,
        {child_available_inline_size, ChildAvailableSize().block_size},
        /* is_new_fc */ true, opportunity.rect.start_offset.block_offset);

    // All formatting context roots (like this child) should start with an empty
    // exclusion space.
    DCHECK(child_space.ExclusionSpace().IsEmpty());

    scoped_refptr<const NGLayoutResult> layout_result = LayoutBlockChild(
        child_space, child_break_token, early_break_, &To<NGBlockNode>(child));

    // Since this child establishes a new formatting context, no exclusion space
    // should be returned.
    DCHECK(layout_result->ExclusionSpace().IsEmpty());

    if (layout_result->Status() != NGLayoutResult::kSuccess) {
      DCHECK_EQ(layout_result->Status(),
                NGLayoutResult::kOutOfFragmentainerSpace);
      return layout_result;
    }

    NGFragment fragment(writing_direction, layout_result->PhysicalFragment());

    // Check if the fragment will fit in this layout opportunity, if not proceed
    // to the next opportunity.
    if ((fragment.InlineSize() > opportunity.rect.InlineSize() &&
         !can_expand_outside_opportunity) ||
        fragment.BlockSize() > opportunity.rect.BlockSize())
      continue;

    // Now find the fragment's (final) position calculating the auto margins.
    NGBoxStrut auto_margins = child_data.margins;
    if (child.IsListMarker()) {
      // Deal with marker's margin. It happens only when marker needs to occupy
      // the whole line.
      DCHECK(child.ListMarkerOccupiesWholeLine());
      // Because the marker is laid out as a normal block child, its inline
      // size is extended to fill up the space. Compute the regular marker size
      // from the first child.
      const auto& marker_fragment = layout_result->PhysicalFragment();
      LayoutUnit marker_inline_size;
      if (!marker_fragment.Children().empty()) {
        marker_inline_size =
            NGFragment(writing_direction, *marker_fragment.Children().front())
                .InlineSize();
      }
      auto_margins.inline_start =
          NGUnpositionedListMarker(To<NGBlockNode>(child))
              .InlineOffset(marker_inline_size);
      auto_margins.inline_end = opportunity.rect.InlineSize() -
                                fragment.InlineSize() -
                                auto_margins.inline_start;
    } else {
      ResolveInlineMargins(child_style, Style(), child_available_inline_size,
                           fragment.InlineSize(), &auto_margins);
    }

    // |auto_margins| are initialized as a copy of the child's initial margins.
    // To determine the effect of the auto-margins we only apply the difference.
    LayoutUnit auto_margin_line_left =
        auto_margins.LineLeft(direction) - line_left_margin;

    *out_child_bfc_offset = {line_left_offset + auto_margin_line_left,
                             opportunity.rect.start_offset.block_offset};
    return layout_result;
  }

  NOTREACHED();
  return nullptr;
}

NGLayoutResult::EStatus NGBlockLayoutAlgorithm::HandleInflow(
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token,
    NGPreviousInflowPosition* previous_inflow_position,
    NGInlineChildLayoutContext* inline_child_layout_context,
    scoped_refptr<const NGInlineBreakToken>* previous_inline_break_token) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK(!child.IsOutOfFlowPositioned());
  DCHECK(!child.CreatesNewFormattingContext());

  bool is_non_empty_inline = false;
  auto* child_inline_node = DynamicTo<NGInlineNode>(child);
  if (child_inline_node) {
    is_non_empty_inline = !child_inline_node->IsEmptyInline();

    // Add reusable line boxes from |previous_result_| if any.
    if (is_non_empty_inline && !child_break_token && previous_result_ &&
        RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
      if (!ResolveBfcBlockOffset(previous_inflow_position))
        return NGLayoutResult::kBfcBlockOffsetResolved;
      DCHECK(container_builder_.BfcBlockOffset());

      DCHECK(!*previous_inline_break_token);
      if (TryReuseFragmentsFromCache(*child_inline_node,
                                     previous_inflow_position,
                                     previous_inline_break_token))
        return NGLayoutResult::kSuccess;
    }
  }

  bool has_clearance_past_adjoining_floats =
      !container_builder_.BfcBlockOffset() && child.IsBlock() &&
      HasClearancePastAdjoiningFloats(container_builder_.AdjoiningObjectTypes(),
                                      child.Style(), Style());

  base::Optional<LayoutUnit> forced_bfc_block_offset;

  // If we can separate the previous margin strut from what is to follow, do
  // that. Then we're able to resolve *our* BFC block offset and position any
  // pending floats. There are two situations where this is necessary:
  //  1. If the child is to be cleared by adjoining floats.
  //  2. If the child is a non-empty inline.
  //
  // Note this logic is copied to TryReuseFragmentsFromCache(), they need to
  // keep in sync.
  if (has_clearance_past_adjoining_floats || is_non_empty_inline) {
    if (!ResolveBfcBlockOffset(previous_inflow_position))
      return NGLayoutResult::kBfcBlockOffsetResolved;

    // If we had clearance past any adjoining floats, we already know where the
    // child is going to be (the child's margins won't have any effect).
    //
    // Set the forced BFC block-offset to the appropriate clearance offset to
    // force this placement of this child.
    if (has_clearance_past_adjoining_floats) {
      forced_bfc_block_offset =
          exclusion_space_.ClearanceOffset(child.Style().Clear(Style()));
    }
  }

  // Perform layout on the child.
  NGInflowChildData child_data =
      ComputeChildData(*previous_inflow_position, child, child_break_token,
                       /* is_new_fc */ false);
  NGConstraintSpace child_space = CreateConstraintSpaceForChild(
      child, child_data, ChildAvailableSize(), /* is_new_fc */ false,
      forced_bfc_block_offset, has_clearance_past_adjoining_floats,
      previous_inflow_position->block_end_annotation_space);
  scoped_refptr<const NGLayoutResult> layout_result =
      LayoutInflow(child_space, child_break_token, early_break_, &child,
                   inline_child_layout_context);

  // To save space of the stack when we recurse into |NGBlockNode::Layout|
  // above, the rest of this function is continued within |FinishInflow|.
  // However it should be read as one function.
  return FinishInflow(child, child_break_token, child_space,
                      has_clearance_past_adjoining_floats,
                      std::move(layout_result), &child_data,
                      previous_inflow_position, inline_child_layout_context,
                      previous_inline_break_token);
}

NGLayoutResult::EStatus NGBlockLayoutAlgorithm::FinishInflow(
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token,
    const NGConstraintSpace& child_space,
    bool has_clearance_past_adjoining_floats,
    scoped_refptr<const NGLayoutResult> layout_result,
    NGInflowChildData* child_data,
    NGPreviousInflowPosition* previous_inflow_position,
    NGInlineChildLayoutContext* inline_child_layout_context,
    scoped_refptr<const NGInlineBreakToken>* previous_inline_break_token) {
  base::Optional<LayoutUnit> child_bfc_block_offset =
      layout_result->BfcBlockOffset();

  bool is_self_collapsing = layout_result->IsSelfCollapsing();

  // Only non self-collapsing children (e.g. "normal children") can be pushed
  // by floats in this way.
  bool normal_child_had_clearance =
      layout_result->IsPushedByFloats() && child.IsBlock();
  DCHECK(!normal_child_had_clearance || !is_self_collapsing);

  // A child may have aborted its layout if it resolved its BFC block-offset.
  // If we don't have a BFC block-offset yet, we need to propagate the abort
  // signal up to our parent.
  if (layout_result->Status() == NGLayoutResult::kBfcBlockOffsetResolved &&
      !container_builder_.BfcBlockOffset()) {
    // There's no need to do anything apart from resolving the BFC block-offset
    // here, so make sure that it aborts before trying to position floats or
    // anything like that, which would just be waste of time.
    //
    // This is simply propagating an abort up to a node which is able to
    // restart the layout (a node that has resolved its BFC block-offset).
    DCHECK(child_bfc_block_offset);
    abort_when_bfc_block_offset_updated_ = true;

    LayoutUnit bfc_block_offset = *child_bfc_block_offset;

    if (normal_child_had_clearance) {
      // If the child has the same clearance-offset as ourselves it means that
      // we should *also* resolve ourselves at that offset, (and we also have
      // been pushed by floats).
      if (ConstraintSpace().ClearanceOffset() == child_space.ClearanceOffset())
        container_builder_.SetIsPushedByFloats();
      else
        bfc_block_offset = NextBorderEdge(*previous_inflow_position);
    }

    // A new formatting-context may have previously tried to resolve the BFC
    // block-offset. In this case we'll have a "forced" BFC block-offset
    // present, but we shouldn't apply it (instead preferring the child's new
    // BFC block-offset).
    DCHECK(!ConstraintSpace().AncestorHasClearancePastAdjoiningFloats());

    if (!ResolveBfcBlockOffset(previous_inflow_position, bfc_block_offset,
                               /* forced_bfc_block_offset */ base::nullopt))
      return NGLayoutResult::kBfcBlockOffsetResolved;
  }

  // We have special behaviour for a self-collapsing child which gets pushed
  // down due to clearance, see comment inside |ComputeInflowPosition|.
  bool self_collapsing_child_had_clearance =
      is_self_collapsing && has_clearance_past_adjoining_floats;

  // We try and position the child within the block formatting-context. This
  // may cause our BFC block-offset to be resolved, in which case we should
  // abort our layout if needed.
  if (!child_bfc_block_offset) {
    DCHECK(is_self_collapsing);
    if (child_space.HasClearanceOffset() && child.Style().HasClear()) {
      // This is a self-collapsing child that we collapsed through, so we have
      // to detect clearance manually. See if the child's hypothetical border
      // edge is past the relevant floats. If it's not, we need to apply
      // clearance before it.
      LayoutUnit child_block_offset_estimate =
          BfcBlockOffset() + layout_result->EndMarginStrut().Sum();
      if (child_block_offset_estimate < child_space.ClearanceOffset())
        self_collapsing_child_had_clearance = true;
    }
  }

  bool child_had_clearance =
      self_collapsing_child_had_clearance || normal_child_had_clearance;
  if (child_had_clearance) {
    // The child has clearance. Clearance inhibits margin collapsing and acts as
    // spacing before the block-start margin of the child. Our BFC block offset
    // is therefore resolvable, and if it hasn't already been resolved, we'll
    // do it now to separate the child's collapsed margin from this container.
    if (!ResolveBfcBlockOffset(previous_inflow_position))
      return NGLayoutResult::kBfcBlockOffsetResolved;
  } else if (layout_result->SubtreeModifiedMarginStrut()) {
    // The child doesn't have clearance, and modified its incoming
    // margin-strut. Propagate this information up to our parent if needed.
    SetSubtreeModifiedMarginStrutIfNeeded();
  }

  bool self_collapsing_child_needs_relayout = false;
  if (!child_bfc_block_offset) {
    // Layout wasn't able to determine the BFC block-offset of the child. This
    // has to mean that the child is self-collapsing.
    DCHECK(is_self_collapsing);

    if (container_builder_.BfcBlockOffset()) {
      // Since we know our own BFC block-offset, though, we can calculate that
      // of the child as well.
      child_bfc_block_offset = PositionSelfCollapsingChildWithParentBfc(
          child, child_space, *child_data, *layout_result);

      // We may need to relayout this child if it had any (adjoining) objects
      // which were positioned in the incorrect place.
      if (layout_result->PhysicalFragment().HasAdjoiningObjectDescendants() &&
          *child_bfc_block_offset != child_space.ExpectedBfcBlockOffset())
        self_collapsing_child_needs_relayout = true;
    }
  } else if (!child_had_clearance && !is_self_collapsing) {
    // Only non self-collapsing children are allowed resolve their parent's BFC
    // block-offset. We check the BFC block-offset at the end of layout
    // determine if this fragment is self-collapsing.
    //
    // The child's BFC block-offset is known, and since there's no clearance,
    // this container will get the same offset, unless it has already been
    // resolved.
    if (!ResolveBfcBlockOffset(previous_inflow_position,
                               *child_bfc_block_offset))
      return NGLayoutResult::kBfcBlockOffsetResolved;
  }

  // We need to re-layout a self-collapsing child if it was affected by
  // clearance in order to produce a new margin strut. For example:
  // <div style="margin-bottom: 50px;"></div>
  // <div id="float" style="height: 50px;"></div>
  // <div id="zero" style="clear: left; margin-top: -20px;">
  //   <div id="zero-inner" style="margin-top: 40px; margin-bottom: -30px;">
  // </div>
  //
  // The end margin strut for #zero will be {50, -30}. #zero will be affected
  // by clearance (as 50 > {50, -30}).
  //
  // As #zero doesn't touch the incoming margin strut now we need to perform a
  // relayout with an empty incoming margin strut.
  //
  // The resulting margin strut in the above example will be {40, -30}. See
  // |ComputeInflowPosition| for how this end margin strut is used.
  if (self_collapsing_child_had_clearance) {
    NGMarginStrut margin_strut;
    margin_strut.Append(child_data->margins.block_start,
                        child.Style().HasMarginBeforeQuirk());

    // We only need to relayout if the new margin strut is different to the
    // previous one.
    if (child_data->margin_strut != margin_strut) {
      child_data->margin_strut = margin_strut;
      self_collapsing_child_needs_relayout = true;
    }
  }

  // We need to layout a child if we know its BFC block offset and:
  //  - It aborted its layout as it resolved its BFC block offset.
  //  - It has some unpositioned floats.
  //  - It was affected by clearance.
  if ((layout_result->Status() == NGLayoutResult::kBfcBlockOffsetResolved ||
       self_collapsing_child_needs_relayout) &&
      child_bfc_block_offset) {
    NGConstraintSpace new_child_space = CreateConstraintSpaceForChild(
        child, *child_data, ChildAvailableSize(), /* is_new_fc */ false,
        child_bfc_block_offset);
    layout_result =
        LayoutInflow(new_child_space, child_break_token, early_break_, &child,
                     inline_child_layout_context);

    if (layout_result->Status() == NGLayoutResult::kBfcBlockOffsetResolved) {
      // Even a second layout pass may abort, if the BFC block offset initially
      // calculated turned out to be wrong. This happens when we discover that
      // an in-flow block-level descendant that establishes a new formatting
      // context doesn't fit beside the floats at its initial position. Allow
      // one more pass.
      child_bfc_block_offset = layout_result->BfcBlockOffset();
      DCHECK(child_bfc_block_offset);
      new_child_space = CreateConstraintSpaceForChild(
          child, *child_data, ChildAvailableSize(), /* is_new_fc */ false,
          child_bfc_block_offset);
      layout_result =
          LayoutInflow(new_child_space, child_break_token, early_break_, &child,
                       inline_child_layout_context);
    }

    DCHECK_EQ(layout_result->Status(), NGLayoutResult::kSuccess);
  }

  // It is now safe to update our version of the exclusion space, and any
  // propagated adjoining floats.
  exclusion_space_ = layout_result->ExclusionSpace();

  // Only self-collapsing children should have adjoining objects.
  DCHECK(!layout_result->AdjoiningObjectTypes() || is_self_collapsing);
  container_builder_.SetAdjoiningObjectTypes(
      layout_result->AdjoiningObjectTypes());

  // If we don't know our BFC block-offset yet, and the child stumbled into
  // something that needs it (unable to position floats yet), we need abort
  // layout, and trigger a re-layout once we manage to resolve it.
  //
  // NOTE: This check is performed after the optional second layout pass above,
  // since we may have been able to resolve our BFC block-offset (e.g. due to
  // clearance) and position any descendant floats in the second pass.
  // In particular, when it comes to clearance of self-collapsing children, if
  // we just applied it and resolved the BFC block-offset to separate the
  // margins before and after clearance, we cannot abort and re-layout this
  // child, or clearance would be lost.
  //
  // If we are a new formatting context, the child will get re-laid out once it
  // has been positioned.
  if (!container_builder_.BfcBlockOffset()) {
    abort_when_bfc_block_offset_updated_ |=
        layout_result->AdjoiningObjectTypes();
    // If our BFC block offset is unknown, and the child got pushed down by
    // floats, so will we.
    if (layout_result->IsPushedByFloats())
      container_builder_.SetIsPushedByFloats();
  }

  const auto& physical_fragment = layout_result->PhysicalFragment();
  NGFragment fragment(ConstraintSpace().GetWritingDirection(),
                      physical_fragment);

  LogicalOffset logical_offset = CalculateLogicalOffset(
      fragment, layout_result->BfcLineOffset(), child_bfc_block_offset);
  if (UNLIKELY(child.IsSliderThumb()))
    logical_offset = AdjustSliderThumbInlineOffset(fragment, logical_offset);

  if (ConstraintSpace().HasBlockFragmentation() &&
      container_builder_.BfcBlockOffset() && child_bfc_block_offset) {
    // Floats only cause container separation for the outermost block child that
    // gets pushed down (the container and the child may have adjoining
    // block-start margins).
    bool has_container_separation =
        has_processed_first_child_ || (layout_result->IsPushedByFloats() &&
                                       !container_builder_.IsPushedByFloats());
    NGBreakStatus break_status = BreakBeforeChildIfNeeded(
        child, *layout_result, previous_inflow_position,
        *child_bfc_block_offset, has_container_separation);
    if (break_status == NGBreakStatus::kBrokeBefore)
      return NGLayoutResult::kSuccess;
    if (break_status == NGBreakStatus::kNeedsEarlierBreak)
      return NGLayoutResult::kNeedsEarlierBreak;
    EBreakBetween break_after = JoinFragmentainerBreakValues(
        layout_result->FinalBreakAfter(), child.Style().BreakAfter());
    container_builder_.SetPreviousBreakAfter(break_after);

    if (inline_child_layout_context) {
      for (auto token : inline_child_layout_context->PropagatedBreakTokens()) {
        container_builder_.AddBreakToken(std::move(token),
                                         /* is_in_parallel_flow */ true);
      }
      inline_child_layout_context->ClearPropagatedBreakTokens();
    }
  }

  if (!PositionOrPropagateListMarker(*layout_result, &logical_offset,
                                     previous_inflow_position))
    return NGLayoutResult::kBfcBlockOffsetResolved;

  // The box with -internal-align-self:center should create new
  // formatting context.
  DCHECK(child.IsInline() || !child.Style().AlignSelfBlockCenter());

  PropagateBaselineFromChild(physical_fragment, logical_offset.block_offset);
  container_builder_.AddResult(*layout_result, logical_offset);

  if (auto* block_child = DynamicTo<NGBlockNode>(child)) {
    // We haven't yet resolved margins wrt. overconstrainedness, unless that was
    // also required to calculate line-left offset (due to block alignment)
    // before layout. Do so now, so that we store the correct values (which is
    // required by e.g. getComputedStyle()).
    if (!child_data->margins_fully_resolved) {
      ResolveInlineMargins(child.Style(), Style(),
                           ChildAvailableSize().inline_size,
                           fragment.InlineSize(), &child_data->margins);
      child_data->margins_fully_resolved = true;
    }

    block_child->StoreMargins(ConstraintSpace(), child_data->margins);
  }

  *previous_inflow_position = ComputeInflowPosition(
      *previous_inflow_position, child, *child_data, child_bfc_block_offset,
      logical_offset, *layout_result, fragment,
      self_collapsing_child_had_clearance);

  *previous_inline_break_token =
      child.IsInline() ? To<NGInlineBreakToken>(physical_fragment.BreakToken())
                       : nullptr;

  // If a spanner was found inside the child, we need to finish up and propagate
  // the spanner to the column layout algorithm, so that it can take care of it.
  if (UNLIKELY(ConstraintSpace().IsInColumnBfc())) {
    if (NGBlockNode spanner_node = layout_result->ColumnSpanner()) {
      DCHECK(container_builder_.HasInflowChildBreakInside());
      container_builder_.SetColumnSpanner(spanner_node);
    }
  }

  // Update |lines_until_clamp_| from the LayoutResult.
  if (lines_until_clamp_) {
    if (const auto* line_box =
            DynamicTo<NGPhysicalLineBoxFragment>(physical_fragment)) {
      if (!line_box->IsEmptyLineBox())
        lines_until_clamp_ = *lines_until_clamp_ - 1;
    } else {
      lines_until_clamp_ = layout_result->LinesUntilClamp();
    }
    if (lines_until_clamp_ <= 0 &&
        !intrinsic_block_size_when_clamped_.has_value()) {
      // If line-clamping occurred save the intrinsic block-size, as this
      // becomes the final intrinsic block-size.
      intrinsic_block_size_when_clamped_ =
          previous_inflow_position->logical_block_offset;
    }
  }
  return NGLayoutResult::kSuccess;
}

NGInflowChildData NGBlockLayoutAlgorithm::ComputeChildData(
    const NGPreviousInflowPosition& previous_inflow_position,
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token,
    bool is_new_fc) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK_EQ(is_new_fc, child.CreatesNewFormattingContext());

  // Calculate margins in parent's writing mode.
  bool margins_fully_resolved;
  NGBoxStrut margins =
      CalculateMargins(child, is_new_fc, &margins_fully_resolved);

  // Append the current margin strut with child's block start margin.
  // Non empty border/padding, and new formatting-context use cases are handled
  // inside of the child's layout
  NGMarginStrut margin_strut = previous_inflow_position.margin_strut;

  const auto* child_block_break_token =
      DynamicTo<NGBlockBreakToken>(child_break_token);
  if (UNLIKELY(child_block_break_token)) {
    AdjustMarginsForFragmentation(child_block_break_token, &margins);
    if (child_block_break_token->IsForcedBreak()) {
      // After a forced fragmentainer break we need to reset the margin strut,
      // in case it was set to discard all margins (which is the default at
      // breaks). Margins after a forced break should be retained.
      margin_strut = NGMarginStrut();
    }
  }

  LayoutUnit logical_block_offset =
      previous_inflow_position.logical_block_offset;

  margin_strut.Append(margins.block_start,
                      child.Style().HasMarginBeforeQuirk());
  SetSubtreeModifiedMarginStrutIfNeeded(&child.Style().MarginBefore());

  NGBfcOffset child_bfc_offset = {
      ConstraintSpace().BfcOffset().line_offset +
          BorderScrollbarPadding().LineLeft(ConstraintSpace().Direction()) +
          margins.LineLeft(ConstraintSpace().Direction()),
      BfcBlockOffset() + logical_block_offset};

  return {child_bfc_offset, margin_strut, margins, margins_fully_resolved,
          IsResumingLayout(child_block_break_token)};
}

NGPreviousInflowPosition NGBlockLayoutAlgorithm::ComputeInflowPosition(
    const NGPreviousInflowPosition& previous_inflow_position,
    const NGLayoutInputNode child,
    const NGInflowChildData& child_data,
    const base::Optional<LayoutUnit>& child_bfc_block_offset,
    const LogicalOffset& logical_offset,
    const NGLayoutResult& layout_result,
    const NGFragment& fragment,
    bool self_collapsing_child_had_clearance) {
  // Determine the child's end logical offset, for the next child to use.
  LayoutUnit logical_block_offset;

  bool is_self_collapsing = layout_result.IsSelfCollapsing();
  if (is_self_collapsing) {
    // The default behaviour for self-collapsing children is they just pass
    // through the previous inflow position.
    logical_block_offset = previous_inflow_position.logical_block_offset;

    if (self_collapsing_child_had_clearance) {
      // If there's clearance, we must have applied that by now and thus
      // resolved our BFC block-offset.
      DCHECK(container_builder_.BfcBlockOffset());
      DCHECK(child_bfc_block_offset.has_value());

      // If a self-collapsing child was affected by clearance (that is it got
      // pushed down past a float), we need to do something slightly bizarre.
      //
      // Instead of just passing through the previous inflow position, we make
      // the inflow position our new position (which was affected by the
      // float), minus what the margin strut which the self-collapsing child
      // produced.
      //
      // Another way of thinking about this is that when you *add* back the
      // margin strut, you end up with the same position as you started with.
      //
      // This is essentially what the spec refers to as clearance [1], and,
      // while we normally don't have to calculate it directly, in the case of
      // a self-collapsing cleared child like here, we actually have to.
      //
      // We have to calculate clearance for self-collapsing cleared children,
      // because we need the margin that's between the clearance and this block
      // to collapse correctly with subsequent content. This is something that
      // needs to take place after the margin strut preceding and following the
      // clearance have been separated. Clearance may be positive, negative or
      // zero, depending on what it takes to (hypothetically) place this child
      // just below the last relevant float. Since the margins before and after
      // the clearance have been separated, we may have to pull the child back,
      // and that's an example of negative clearance.
      //
      // (In the other case, when a cleared child is non self-collapsing (i.e.
      // when we don't end up here), we don't need to explicitly calculate
      // clearance, because then we just place its border edge where it should
      // be and we're done with it.)
      //
      // [1] https://www.w3.org/TR/CSS22/visuren.html#flow-control

      // First move past the margin that is to precede the clearance. It will
      // not participate in any subsequent margin collapsing.
      LayoutUnit margin_before_clearance =
          previous_inflow_position.margin_strut.Sum();
      logical_block_offset += margin_before_clearance;

      // Calculate and apply actual clearance.
      LayoutUnit clearance = *child_bfc_block_offset -
                             layout_result.EndMarginStrut().Sum() -
                             NextBorderEdge(previous_inflow_position);
      logical_block_offset += clearance;
    }
    if (!container_builder_.BfcBlockOffset())
      DCHECK_EQ(logical_block_offset, LayoutUnit());
  } else {
    // We add AnnotationOverflow unconditionally here.  Then, we cancel it if
    //  - The next line box has block-start annotation space, or
    //  - There are no following child boxes and this container has block-end
    //    padding.
    //
    // See NGInlineLayoutAlgorithm::CreateLine() and
    // BlockLayoutAlgorithm::Layout().
    logical_block_offset = logical_offset.block_offset + fragment.BlockSize() +
                           layout_result.AnnotationOverflow();
  }

  NGMarginStrut margin_strut = layout_result.EndMarginStrut();

  // Self collapsing child's end margin can "inherit" quirkiness from its start
  // margin. E.g.
  // <ol style="margin-bottom: 20px"></ol>
  bool is_quirky =
      (is_self_collapsing && child.Style().HasMarginBeforeQuirk()) ||
      child.Style().HasMarginAfterQuirk();
  margin_strut.Append(child_data.margins.block_end, is_quirky);
  SetSubtreeModifiedMarginStrutIfNeeded(&child.Style().MarginAfter());

  // This flag is subtle, but in order to determine our size correctly we need
  // to check if our last child is self-collapsing, and it was affected by
  // clearance *or* an adjoining self-collapsing sibling was affected by
  // clearance. E.g.
  // <div id="container">
  //   <div id="float"></div>
  //   <div id="zero-with-clearance"></div>
  //   <div id="another-zero"></div>
  // </div>
  // In the above case #container's size will depend on the end margin strut of
  // #another-zero, even though usually it wouldn't.
  bool self_or_sibling_self_collapsing_child_had_clearance =
      self_collapsing_child_had_clearance ||
      (previous_inflow_position.self_collapsing_child_had_clearance &&
       is_self_collapsing);

  LayoutUnit annotation_space = layout_result.BlockEndAnnotationSpace();
  if (layout_result.AnnotationOverflow() > LayoutUnit()) {
    DCHECK(!annotation_space);
    annotation_space = -layout_result.AnnotationOverflow();
  }

  return {logical_block_offset, margin_strut, annotation_space,
          self_or_sibling_self_collapsing_child_had_clearance};
}

LayoutUnit NGBlockLayoutAlgorithm::PositionSelfCollapsingChildWithParentBfc(
    const NGLayoutInputNode& child,
    const NGConstraintSpace& child_space,
    const NGInflowChildData& child_data,
    const NGLayoutResult& layout_result) const {
  DCHECK(layout_result.IsSelfCollapsing());

  // The child must be an in-flow zero-block-size fragment, use its end margin
  // strut for positioning.
  LayoutUnit child_bfc_block_offset =
      child_data.bfc_offset_estimate.block_offset +
      layout_result.EndMarginStrut().Sum();

  ApplyClearance(child_space, &child_bfc_block_offset);

  return child_bfc_block_offset;
}

void NGBlockLayoutAlgorithm::FinalizeForTableCell(
    LayoutUnit unconstrained_intrinsic_block_size) {
  // Hide table-cells if:
  //  - They are within a collapsed column(s).
  //  - They have "empty-cells: hide", non-collapsed borders, and no children.
  container_builder_.SetIsHiddenForPaint(
      ConstraintSpace().IsTableCellHiddenForPaint() ||
      (ConstraintSpace().HideTableCellIfEmpty() &&
       container_builder_.Children().IsEmpty()));

  container_builder_.SetHasCollapsedBorders(
      ConstraintSpace().IsTableCellWithCollapsedBorders());

  // Everything else within this function only applies to new table-cells.
  if (ConstraintSpace().IsLegacyTableCell())
    return;

  container_builder_.SetIsTableNGPart();

  container_builder_.SetTableCellColumnIndex(
      ConstraintSpace().TableCellColumnIndex());

  switch (Style().VerticalAlign()) {
    case EVerticalAlign::kTop:
      // Do nothing for 'top' vertical alignment.
      break;
    case EVerticalAlign::kBaselineMiddle:
    case EVerticalAlign::kSub:
    case EVerticalAlign::kSuper:
    case EVerticalAlign::kTextTop:
    case EVerticalAlign::kTextBottom:
    case EVerticalAlign::kLength:
      // All of the above are treated as 'baseline' for the purposes of
      // table-cell vertical alignment.
    case EVerticalAlign::kBaseline:
      // Table-cells (with baseline vertical alignment) always produce a
      // baseline of their end-content edge (even if the content doesn't have
      // any baselines).
      if (!container_builder_.Baseline() ||
          Node().ShouldApplyLayoutContainment()) {
        container_builder_.SetBaseline(unconstrained_intrinsic_block_size -
                                       BorderScrollbarPadding().block_end);
      }

      if (auto alignment_baseline =
              ConstraintSpace().TableCellAlignmentBaseline()) {
        container_builder_.MoveChildrenInBlockDirection(
            *alignment_baseline - *container_builder_.Baseline());
      }
      break;
    case EVerticalAlign::kMiddle:
      container_builder_.MoveChildrenInBlockDirection(
          (container_builder_.FragmentBlockSize() -
           unconstrained_intrinsic_block_size) /
          2);
      break;
    case EVerticalAlign::kBottom:
      container_builder_.MoveChildrenInBlockDirection(
          container_builder_.FragmentBlockSize() -
          unconstrained_intrinsic_block_size);
      break;
  };
}

LayoutUnit NGBlockLayoutAlgorithm::FragmentainerSpaceAvailable() const {
  DCHECK(container_builder_.BfcBlockOffset());
  return FragmentainerSpaceAtBfcStart(ConstraintSpace()) -
         *container_builder_.BfcBlockOffset();
}

void NGBlockLayoutAlgorithm::ConsumeRemainingFragmentainerSpace(
    NGPreviousInflowPosition* previous_inflow_position) {
  if (ConstraintSpace().HasKnownFragmentainerBlockSize()) {
    // The remaining part of the fragmentainer (the unusable space for child
    // content, due to the break) should still be occupied by this container.
    previous_inflow_position->logical_block_offset =
        FragmentainerSpaceAvailable();
  }
}

bool NGBlockLayoutAlgorithm::FinalizeForFragmentation() {
  if (Node().IsInlineFormattingContextRoot() && !early_break_) {
    if (container_builder_.HasInflowChildBreakInside() ||
        first_overflowing_line_) {
      if (first_overflowing_line_ &&
          first_overflowing_line_ < container_builder_.LineCount()) {
        int line_number;
        if (fit_all_lines_) {
          line_number = first_overflowing_line_;
        } else {
          // We managed to finish layout of all the lines for the node, which
          // means that we won't have enough widows, unless we break earlier
          // than where we overflowed.
          int line_count = container_builder_.LineCount();
          line_number = std::max(line_count - Style().Widows(),
                                 std::min(line_count, int(Style().Orphans())));
        }
        // We need to layout again, and stop at the right line number.
        scoped_refptr<const NGEarlyBreak> breakpoint =
            base::AdoptRef(new NGEarlyBreak(line_number));
        container_builder_.SetEarlyBreak(breakpoint, kBreakAppealPerfect);
        return false;
      }
    } else {
      // Everything could fit in the current fragmentainer, but, depending on
      // what comes after, the best location to break at may be between two of
      // our lines.
      UpdateEarlyBreakBetweenLines();
    }
  }

  if (container_builder_.IsFragmentainerBoxType()) {
    // We're building fragmentainers. Just copy the block-size from the
    // constraint space. Calculating the size the regular way would cause some
    // problems with overflow. For one, we don't want to produce a break token
    // if there's no child content that requires it.
    LayoutUnit consumed_block_size =
        BreakToken() ? BreakToken()->ConsumedBlockSize() : LayoutUnit();
    LayoutUnit block_size = ConstraintSpace().FragmentainerBlockSize();
    container_builder_.SetFragmentBlockSize(block_size);
    container_builder_.SetConsumedBlockSize(consumed_block_size + block_size);
    return true;
  }

  LayoutUnit space_left = kIndefiniteSize;
  if (ConstraintSpace().HasKnownFragmentainerBlockSize())
    space_left = FragmentainerSpaceAvailable();

  return FinishFragmentation(Node(), ConstraintSpace(),
                             BorderPadding().block_end, space_left,
                             &container_builder_);
}

NGBreakStatus NGBlockLayoutAlgorithm::BreakBeforeChildIfNeeded(
    NGLayoutInputNode child,
    const NGLayoutResult& layout_result,
    NGPreviousInflowPosition* previous_inflow_position,
    LayoutUnit bfc_block_offset,
    bool has_container_separation) {
  DCHECK(ConstraintSpace().HasBlockFragmentation());

  // If the BFC offset is unknown, there's nowhere to break, since there's no
  // non-empty child content yet (as that would have resolved the BFC offset).
  DCHECK(container_builder_.BfcBlockOffset());

  // If we already know where to insert the break, we already know that it's not
  // going to be here, since that's something we check before entering layout of
  // a child.
  if (early_break_)
    return NGBreakStatus::kContinue;

  LayoutUnit fragmentainer_block_offset =
      ConstraintSpace().FragmentainerOffsetAtBfc() + bfc_block_offset;

  if (has_container_separation) {
    EBreakBetween break_between =
        CalculateBreakBetweenValue(child, layout_result, container_builder_);
    if (IsForcedBreakValue(ConstraintSpace(), break_between)) {
      BreakBeforeChild(ConstraintSpace(), child, layout_result,
                       fragmentainer_block_offset, kBreakAppealPerfect,
                       /* is_forced_break */ true, &container_builder_);
      ConsumeRemainingFragmentainerSpace(previous_inflow_position);
      return NGBreakStatus::kBrokeBefore;
    }
  }

  NGBreakAppeal appeal_before =
      CalculateBreakAppealBefore(ConstraintSpace(), child, layout_result,
                                 container_builder_, has_container_separation);

  // Attempt to move past the break point, and if we can do that, also assess
  // the appeal of breaking there, even if we didn't.
  if (MovePastBreakpoint(ConstraintSpace(), child, layout_result,
                         fragmentainer_block_offset, appeal_before,
                         &container_builder_))
    return NGBreakStatus::kContinue;

  // Figure out where to insert a soft break. It will either be before this
  // child, or before an earlier sibling, if there's a more appealing breakpoint
  // there.

  // If we decided to insert a soft break, we have to know the fragmentainer
  // block-size.
  DCHECK(ConstraintSpace().HasKnownFragmentainerBlockSize());

  if (child.IsInline()) {
    if (!first_overflowing_line_) {
      // We're at the first overflowing line. This is the space shortage that
      // we are going to report. We do this in spite of not yet knowing
      // whether breaking here would violate orphans and widows requests. This
      // approach may result in a lower space shortage than what's actually
      // true, which leads to more layout passes than we'd otherwise
      // need. However, getting this optimal for orphans and widows would
      // require an additional piece of machinery. This case should be rare
      // enough (to worry about performance), so let's focus on code
      // simplicity instead.
      PropagateSpaceShortage(ConstraintSpace(), layout_result,
                             fragmentainer_block_offset, &container_builder_);
    }
    // Attempt to honor orphans and widows requests.
    if (int line_count = container_builder_.LineCount()) {
      if (!first_overflowing_line_)
        first_overflowing_line_ = line_count;
      bool is_first_fragment = !BreakToken();
      // Figure out how many lines we need before the break. That entails to
      // attempt to honor the orphans request.
      int minimum_line_count = Style().Orphans();
      if (!is_first_fragment) {
        // If this isn't the first fragment, it means that there's a break both
        // before and after this fragment. So what was seen as trailing widows
        // in the previous fragment is essentially orphans for us now.
        minimum_line_count =
            std::max(minimum_line_count, static_cast<int>(Style().Widows()));
      }
      if (line_count < minimum_line_count) {
        // Not enough orphans. Our only hope is if we can break before the start
        // of this block to improve on the situation. That's not something we
        // can determine at this point though. Permit the break, but mark it as
        // undesirable.
        if (appeal_before > kBreakAppealViolatingOrphansAndWidows)
          appeal_before = kBreakAppealViolatingOrphansAndWidows;
      } else {
        // There are enough lines before the break. Try to make sure that
        // there'll be enough lines after the break as well. Attempt to honor
        // the widows request.
        DCHECK_GE(line_count, first_overflowing_line_);
        int widows_found = line_count - first_overflowing_line_ + 1;
        if (widows_found < Style().Widows()) {
          // Although we're out of space, we have to continue layout to figure
          // out exactly where to break in order to honor the widows
          // request. We'll make sure that we're going to leave at least as many
          // lines as specified by the 'widows' property for the next fragment
          // (if at all possible), which means that lines that could fit in the
          // current fragment (that we have already laid out) may have to be
          // saved for the next fragment.
          return NGBreakStatus::kContinue;
        }

        // We have determined that there are plenty of lines for the next
        // fragment, so we can just break exactly where we ran out of space,
        // rather than pushing some of the line boxes over to the next fragment.
      }
      fit_all_lines_ = true;
    }
  }

  if (!AttemptSoftBreak(ConstraintSpace(), child, layout_result,
                        fragmentainer_block_offset, appeal_before,
                        &container_builder_))
    return NGBreakStatus::kNeedsEarlierBreak;

  ConsumeRemainingFragmentainerSpace(previous_inflow_position);
  return NGBreakStatus::kBrokeBefore;
}

void NGBlockLayoutAlgorithm::UpdateEarlyBreakBetweenLines() {
  // We shouldn't be here if we already know where to break.
  DCHECK(!early_break_);

  // If something in this flow already broke, it's a little too late to look for
  // breakpoints.
  DCHECK(!container_builder_.HasInflowChildBreakInside());

  int line_count = container_builder_.LineCount();
  if (line_count < 2)
    return;
  // We can break between two of the lines if we have to. Calculate the best
  // line number to break before, and the appeal of such a breakpoint.
  int line_number =
      std::max(line_count - Style().Widows(),
               std::min(line_count - 1, static_cast<int>(Style().Orphans())));
  NGBreakAppeal appeal = kBreakAppealPerfect;
  if (line_number < Style().Orphans() ||
      line_count - line_number < Style().Widows()) {
    // Not enough lines in this container to satisfy the orphans and/or widows
    // requirement. If we break before the last line (i.e. the last possible
    // class B breakpoint), we'll fit as much as possible, and that's the best
    // we can do.
    line_number = line_count - 1;
    appeal = kBreakAppealViolatingOrphansAndWidows;
  }
  if (container_builder_.BreakAppeal() <= appeal) {
    scoped_refptr<const NGEarlyBreak> breakpoint =
        base::AdoptRef(new NGEarlyBreak(line_number));
    container_builder_.SetEarlyBreak(breakpoint, appeal);
  }
}

NGBoxStrut NGBlockLayoutAlgorithm::CalculateMargins(
    NGLayoutInputNode child,
    bool is_new_fc,
    bool* margins_fully_resolved) {
  // We need to at least partially resolve margins before creating a constraint
  // space for layout. Layout needs to know the line-left offset before
  // starting. If the line-left offset cannot be calculated without fully
  // resolving the margins (because of block alignment), we have to create a
  // temporary constraint space now to figure out the inline size first. In all
  // other cases we'll postpone full resolution until after child layout, when
  // we actually have a child constraint space to use (and know the inline
  // size).
  *margins_fully_resolved = false;

  DCHECK(child);
  if (child.IsInline())
    return {};
  const ComputedStyle& child_style = child.Style();
  bool needs_inline_size =
      NeedsInlineSizeToResolveLineLeft(child_style, Style());
  if (!needs_inline_size && !child_style.MayHaveMargin())
    return {};

  NGBoxStrut margins =
      ComputeMarginsFor(child_style, child_percentage_size_.inline_size,
                        ConstraintSpace().GetWritingDirection());

  // As long as the child isn't establishing a new formatting context, we need
  // to know its line-left offset before layout, to be able to position child
  // floats correctly. If we need to resolve auto margins or other alignment
  // properties to calculate the line-left offset, we also need to calculate its
  // inline size first.
  if (!is_new_fc && needs_inline_size) {
    NGConstraintSpaceBuilder builder(ConstraintSpace(),
                                     child_style.GetWritingDirection(),
                                     /* is_new_fc */ false);
    builder.SetAvailableSize(ChildAvailableSize());
    builder.SetPercentageResolutionSize(child_percentage_size_);
    builder.SetStretchInlineSizeIfAuto(true);
    NGConstraintSpace space = builder.ToConstraintSpace();

    NGBoxStrut child_border_padding =
        ComputeBorders(space, To<NGBlockNode>(child)) +
        ComputePadding(space, child_style);
    LayoutUnit child_inline_size =
        ComputeInlineSizeForFragment(space, child, child_border_padding);

    ResolveInlineMargins(child_style, Style(),
                         space.AvailableSize().inline_size, child_inline_size,
                         &margins);
    *margins_fully_resolved = true;
  }
  return margins;
}

NGConstraintSpace NGBlockLayoutAlgorithm::CreateConstraintSpaceForChild(
    const NGLayoutInputNode child,
    const NGInflowChildData& child_data,
    const LogicalSize child_available_size,
    bool is_new_fc,
    const base::Optional<LayoutUnit> child_bfc_block_offset,
    bool has_clearance_past_adjoining_floats,
    LayoutUnit block_start_annotation_space) {
  const ComputedStyle& style = Style();
  const ComputedStyle& child_style = child.Style();
  const auto child_writing_direction = child.IsInline()
                                           ? style.GetWritingDirection()
                                           : child_style.GetWritingDirection();

  NGConstraintSpaceBuilder builder(ConstraintSpace(), child_writing_direction,
                                   is_new_fc);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), child, &builder);

  if (IsParallelWritingMode(ConstraintSpace().GetWritingMode(),
                            child_writing_direction.GetWritingMode())) {
    if (!child.GetLayoutBox()->AutoWidthShouldFitContent() &&
        !child.IsReplaced())
      builder.SetStretchInlineSizeIfAuto(true);
  }

  builder.SetAvailableSize(child_available_size);
  builder.SetPercentageResolutionSize(child_percentage_size_);
  builder.SetReplacedPercentageResolutionSize(replaced_child_percentage_size_);

  if (ConstraintSpace().IsTableCell()) {
    // If we have a fixed block-size we are in the "layout" mode.
    NGTableCellChildLayoutMode mode;
    if (ConstraintSpace().IsFixedBlockSize())
      mode = NGTableCellChildLayoutMode::kLayout;
    else if (ConstraintSpace().IsRestrictedBlockSizeTableCell())
      mode = NGTableCellChildLayoutMode::kMeasureRestricted;
    else
      mode = NGTableCellChildLayoutMode::kMeasure;

    builder.SetTableCellChildLayoutMode(mode);
  }

  bool has_bfc_block_offset = container_builder_.BfcBlockOffset().has_value();

  // Propagate the |NGConstraintSpace::ForcedBfcBlockOffset| down to our
  // children.
  if (!has_bfc_block_offset && ConstraintSpace().ForcedBfcBlockOffset())
    builder.SetForcedBfcBlockOffset(*ConstraintSpace().ForcedBfcBlockOffset());
  if (child_bfc_block_offset && !is_new_fc)
    builder.SetForcedBfcBlockOffset(*child_bfc_block_offset);

  if (has_bfc_block_offset && child.IsBlock()) {
    // Typically we aren't allowed to look at the previous layout result within
    // a layout algorithm. However this is fine (honest), as it is just a hint
    // to the child algorithm for where floats should be placed. If it doesn't
    // have this flag, or gets this estimate wrong, it'll relayout with the
    // appropriate "forced" BFC block-offset.
    if (const NGLayoutResult* previous_result =
            child.GetLayoutBox()->GetCachedLayoutResult()) {
      const NGConstraintSpace& prev_space =
          previous_result->GetConstraintSpaceForCaching();

      // To increase the hit-rate we adjust the previous "optimistic"/"forced"
      // BFC block-offset by how much the child has shifted from the previous
      // layout.
      LayoutUnit bfc_block_delta = child_data.bfc_offset_estimate.block_offset -
                                   prev_space.BfcOffset().block_offset;
      if (prev_space.ForcedBfcBlockOffset()) {
        builder.SetOptimisticBfcBlockOffset(*prev_space.ForcedBfcBlockOffset() +
                                            bfc_block_delta);
      } else if (prev_space.OptimisticBfcBlockOffset()) {
        builder.SetOptimisticBfcBlockOffset(
            *prev_space.OptimisticBfcBlockOffset() + bfc_block_delta);
      }
    }
  } else if (ConstraintSpace().OptimisticBfcBlockOffset()) {
    // Propagate the |NGConstraintSpace::OptimisticBfcBlockOffset| down to our
    // children.
    builder.SetOptimisticBfcBlockOffset(
        *ConstraintSpace().OptimisticBfcBlockOffset());
  }

  // Propagate the |NGConstraintSpace::AncestorHasClearancePastAdjoiningFloats|
  // flag down to our children.
  if (!has_bfc_block_offset &&
      ConstraintSpace().AncestorHasClearancePastAdjoiningFloats())
    builder.SetAncestorHasClearancePastAdjoiningFloats();
  if (has_clearance_past_adjoining_floats)
    builder.SetAncestorHasClearancePastAdjoiningFloats();

  LayoutUnit clearance_offset = ConstraintSpace().IsNewFormattingContext()
                                    ? LayoutUnit::Min()
                                    : ConstraintSpace().ClearanceOffset();
  if (child.IsBlock()) {
    LayoutUnit child_clearance_offset =
        exclusion_space_.ClearanceOffset(child_style.Clear(Style()));
    clearance_offset = std::max(clearance_offset, child_clearance_offset);

    // |PositionListMarker()| requires a baseline.
    builder.SetBaselineAlgorithmType(ConstraintSpace().BaselineAlgorithmType());
  }
  builder.SetClearanceOffset(clearance_offset);

  if (!is_new_fc) {
    builder.SetMarginStrut(child_data.margin_strut);
    builder.SetBfcOffset(child_data.bfc_offset_estimate);
    builder.SetExclusionSpace(exclusion_space_);
    if (!has_bfc_block_offset) {
      builder.SetAdjoiningObjectTypes(
          container_builder_.AdjoiningObjectTypes());
    }
    builder.SetIsLineClampContext(is_line_clamp_context_);
    builder.SetLinesUntilClamp(lines_until_clamp_);
  } else if (child_data.allow_discard_start_margin) {
    // If the child is being resumed after a break, margins inside the child may
    // be adjoining with the fragmentainer boundary, regardless of whether the
    // child establishes a new formatting context or not.
    builder.SetDiscardingMarginStrut();
  }
  builder.SetBlockStartAnnotationSpace(block_start_annotation_space);

  if (ConstraintSpace().HasBlockFragmentation()) {
    LayoutUnit fragmentainer_offset_delta;
    // If a block establishes a new formatting context, we must know our
    // position in the formatting context, to be able to adjust the
    // fragmentation line.
    if (is_new_fc)
      fragmentainer_offset_delta = *child_bfc_block_offset;
    SetupSpaceBuilderForFragmentation(ConstraintSpace(), child,
                                      fragmentainer_offset_delta, &builder,
                                      is_new_fc);
    builder.SetEarlyBreakAppeal(container_builder_.BreakAppeal());
  }

  return builder.ToConstraintSpace();
}

void NGBlockLayoutAlgorithm::PropagateBaselineFromChild(
    const NGPhysicalContainerFragment& child,
    LayoutUnit block_offset) {
  // Check if we've already found an appropriate baseline.
  if (container_builder_.Baseline() &&
      ConstraintSpace().BaselineAlgorithmType() ==
          NGBaselineAlgorithmType::kFirstLine)
    return;

  if (child.IsLineBox()) {
    const auto& line_box = To<NGPhysicalLineBoxFragment>(child);

    // Skip over a line-box which is empty. These don't have any baselines
    // which should be added.
    if (line_box.IsEmptyLineBox())
      return;

    FontHeight metrics = line_box.BaselineMetrics();
    DCHECK(!metrics.IsEmpty());
    LayoutUnit baseline =
        block_offset + (Style().IsFlippedLinesWritingMode() ? metrics.descent
                                                            : metrics.ascent);

    if (!container_builder_.Baseline())
      container_builder_.SetBaseline(baseline);

    // Set the last baseline only if required.
    if (ConstraintSpace().BaselineAlgorithmType() !=
        NGBaselineAlgorithmType::kFirstLine)
      container_builder_.SetLastBaseline(baseline);

    return;
  }

  NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                         To<NGPhysicalBoxFragment>(child));

  if (!container_builder_.Baseline()) {
    if (auto baseline = fragment.FirstBaseline())
      container_builder_.SetBaseline(block_offset + *baseline);
  }

  // Set the last baseline only if required.
  if (ConstraintSpace().BaselineAlgorithmType() !=
      NGBaselineAlgorithmType::kFirstLine) {
    if (auto last_baseline = fragment.Baseline())
      container_builder_.SetLastBaseline(block_offset + *last_baseline);
  }
}

bool NGBlockLayoutAlgorithm::ResolveBfcBlockOffset(
    NGPreviousInflowPosition* previous_inflow_position,
    LayoutUnit bfc_block_offset,
    base::Optional<LayoutUnit> forced_bfc_block_offset) {
  if (container_builder_.BfcBlockOffset())
    return true;

  bfc_block_offset = forced_bfc_block_offset.value_or(bfc_block_offset);

  if (ApplyClearance(ConstraintSpace(), &bfc_block_offset))
    container_builder_.SetIsPushedByFloats();

  container_builder_.SetBfcBlockOffset(bfc_block_offset);

  if (NeedsAbortOnBfcBlockOffsetChange())
    return false;

  // Set the offset to our block-start border edge. We'll now end up at the
  // block-start border edge. If the BFC block offset was resolved due to a
  // block-start border or padding, that must be added by the caller, for
  // subsequent layout to continue at the right position. Whether we need to add
  // border+padding or not isn't something we should determine here, so it must
  // be dealt with as part of initializing the layout algorithm.
  previous_inflow_position->logical_block_offset = LayoutUnit();

  // Resolving the BFC offset normally means that we have finished collapsing
  // adjoining margins, so that we can reset the margin strut. One exception
  // here is if we're resuming after a break, in which case we know that we can
  // resolve the BFC offset to the block-start of the fragmentainer
  // (block-offset 0). But keep the margin strut, since we're essentially still
  // collapsing with the fragmentainer boundary, which will eat / discard all
  // adjoining margins - unless this is at a forced break. DCHECK that the strut
  // is empty (note that a strut that's set up to eat all margins will also be
  // considered to be empty).
  if (!is_resuming_)
    previous_inflow_position->margin_strut = NGMarginStrut();
  else
    DCHECK(previous_inflow_position->margin_strut.IsEmpty());

  return true;
}

bool NGBlockLayoutAlgorithm::NeedsAbortOnBfcBlockOffsetChange() const {
  DCHECK(container_builder_.BfcBlockOffset());
  if (!abort_when_bfc_block_offset_updated_)
    return false;

  // If our position differs from our (potentially optimistic) estimate, abort.
  return *container_builder_.BfcBlockOffset() !=
         ConstraintSpace().ExpectedBfcBlockOffset();
}

base::Optional<LayoutUnit>
NGBlockLayoutAlgorithm::CalculateQuirkyBodyMarginBlockSum(
    const NGMarginStrut& end_margin_strut) {
  if (!Node().IsQuirkyAndFillsViewport())
    return base::nullopt;

  if (!Style().LogicalHeight().IsAuto())
    return base::nullopt;

  if (ConstraintSpace().IsNewFormattingContext())
    return base::nullopt;

  DCHECK(Node().IsBody());
  LayoutUnit block_end_margin =
      ComputeMarginsForSelf(ConstraintSpace(), Style()).block_end;

  // The |end_margin_strut| is the block-start margin if the body doesn't have
  // a resolved BFC block-offset.
  if (!container_builder_.BfcBlockOffset())
    return end_margin_strut.Sum() + block_end_margin;

  NGMarginStrut body_strut = end_margin_strut;
  body_strut.Append(block_end_margin, Style().HasMarginAfterQuirk());
  return *container_builder_.BfcBlockOffset() -
         ConstraintSpace().BfcOffset().block_offset + body_strut.Sum();
}

bool NGBlockLayoutAlgorithm::PositionOrPropagateListMarker(
    const NGLayoutResult& layout_result,
    LogicalOffset* content_offset,
    NGPreviousInflowPosition* previous_inflow_position) {
  // If this is not a list-item, propagate unpositioned list markers to
  // ancestors.
  if (!node_.IsListItem()) {
    if (layout_result.UnpositionedListMarker()) {
      DCHECK(!container_builder_.UnpositionedListMarker());
      container_builder_.SetUnpositionedListMarker(
          layout_result.UnpositionedListMarker());
    }
    return true;
  }

  // If this is a list item, add the unpositioned list marker as a child.
  NGUnpositionedListMarker list_marker = layout_result.UnpositionedListMarker();
  if (!list_marker) {
    list_marker = container_builder_.UnpositionedListMarker();
    if (!list_marker)
      return true;
    container_builder_.SetUnpositionedListMarker(NGUnpositionedListMarker());
  }

  const NGConstraintSpace& space = ConstraintSpace();
  const NGPhysicalFragment& content = layout_result.PhysicalFragment();
  FontBaseline baseline_type = Style().GetFontBaseline();
  if (auto content_baseline =
          list_marker.ContentAlignmentBaseline(space, baseline_type, content)) {
    // TODO: We are reusing the ConstraintSpace for LI here. It works well for
    // now because authors cannot style list-markers currently. If we want to
    // support `::marker` pseudo, we need to create ConstraintSpace for marker
    // separately.
    scoped_refptr<const NGLayoutResult> marker_layout_result =
        list_marker.Layout(space, container_builder_.Style(), baseline_type);
    DCHECK(marker_layout_result);
    // If the BFC block-offset of li is still not resolved, resolved it now.
    if (!container_builder_.BfcBlockOffset() &&
        marker_layout_result->BfcBlockOffset()) {
      // TODO: Currently the margin-top of marker is always zero. To support
      // `::marker` pseudo, we should count marker's margin-top in.
#if DCHECK_IS_ON()
      list_marker.CheckMargin();
#endif
      if (!ResolveBfcBlockOffset(previous_inflow_position))
        return false;
    }

    list_marker.AddToBox(space, baseline_type, content,
                         BorderScrollbarPadding(), *marker_layout_result,
                         *content_baseline, content_offset,
                         &container_builder_);
    return true;
  }

  // If the list marker could not be positioned against this child because it
  // does not have the baseline to align to, keep it as unpositioned and try
  // the next child.
  container_builder_.SetUnpositionedListMarker(list_marker);
  return true;
}

bool NGBlockLayoutAlgorithm::PositionListMarkerWithoutLineBoxes(
    NGPreviousInflowPosition* previous_inflow_position) {
  DCHECK(node_.IsListItem());
  DCHECK(container_builder_.UnpositionedListMarker());

  NGUnpositionedListMarker list_marker =
      container_builder_.UnpositionedListMarker();
  const NGConstraintSpace& space = ConstraintSpace();
  FontBaseline baseline_type = Style().GetFontBaseline();
  // Layout the list marker.
  scoped_refptr<const NGLayoutResult> marker_layout_result =
      list_marker.Layout(space, container_builder_.Style(), baseline_type);
  DCHECK(marker_layout_result);
  // If the BFC block-offset of li is still not resolved, resolve it now.
  if (!container_builder_.BfcBlockOffset() &&
      marker_layout_result->BfcBlockOffset()) {
    // TODO: Currently the margin-top of marker is always zero. To support
    // `::marker` pseudo, we should count marker's margin-top in.
#if DCHECK_IS_ON()
    list_marker.CheckMargin();
#endif
    if (!ResolveBfcBlockOffset(previous_inflow_position))
      return false;
  }
  // Position the list marker without aligning to line boxes.
  LayoutUnit marker_block_size = list_marker.AddToBoxWithoutLineBoxes(
      space, baseline_type, *marker_layout_result, &container_builder_);
  container_builder_.SetUnpositionedListMarker(NGUnpositionedListMarker());

  // Whether the list marker should affect the block size or not is not
  // well-defined, but 3 out of 4 impls do.
  // https://github.com/w3c/csswg-drafts/issues/2418
  //
  // The BFC block-offset has been resolved after layout marker. We'll always
  // include the marker into the block-size.
  if (container_builder_.BfcBlockOffset()) {
    intrinsic_block_size_ = std::max(marker_block_size, intrinsic_block_size_);
    container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
    container_builder_.SetFragmentsTotalBlockSize(
        std::max(marker_block_size, container_builder_.Size().block_size));
  }
  return true;
}

bool NGBlockLayoutAlgorithm::IsRubyText(const NGLayoutInputNode& child) const {
  return Node().IsRubyRun() && child.IsRubyText();
}

void NGBlockLayoutAlgorithm::HandleRubyText(NGBlockNode ruby_text_child) {
  DCHECK(Node().IsRubyRun());

  scoped_refptr<const NGBlockBreakToken> break_token;
  if (const auto* token = BreakToken()) {
    for (const auto* child_token : token->ChildBreakTokens()) {
      if (child_token->InputNode() == ruby_text_child) {
        break_token = To<NGBlockBreakToken>(child_token);
        break;
      }
    }
  }

  const ComputedStyle& rt_style = ruby_text_child.Style();
  NGConstraintSpaceBuilder builder(ConstraintSpace(),
                                   rt_style.GetWritingDirection(), true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), ruby_text_child, &builder);
  builder.SetAvailableSize(ChildAvailableSize());
  if (IsParallelWritingMode(ConstraintSpace().GetWritingMode(),
                            rt_style.GetWritingMode()))
    builder.SetStretchInlineSizeIfAuto(true);

  scoped_refptr<const NGLayoutResult> result =
      ruby_text_child.Layout(builder.ToConstraintSpace(), break_token.get());

  LayoutUnit ruby_text_box_top;
  const NGPhysicalBoxFragment& ruby_text_fragment =
      To<NGPhysicalBoxFragment>(result->PhysicalFragment());
  const LogicalRect ruby_text_box = ruby_text_fragment.ConvertChildToLogical(
      ruby_text_fragment.ScrollableOverflow(NGPhysicalFragment::kEmHeight));
  RubyPosition block_start_position = Style().IsFlippedLinesWritingMode()
                                          ? RubyPosition::kAfter
                                          : RubyPosition::kBefore;
  if (Style().GetRubyPosition() == block_start_position) {
    LayoutUnit last_line_ruby_text_bottom = ruby_text_box.BlockEndOffset();

    // Find a fragment for RubyBase, and get the top of text in it.
    LayoutUnit first_line_top;
    for (const auto& child : container_builder_.Children()) {
      if (const auto* layout_object = child.fragment->GetLayoutObject()) {
        if (layout_object->IsRubyBase()) {
          const auto& ruby_base_fragment =
              To<NGPhysicalBoxFragment>(*child.fragment);
          first_line_top =
              ruby_base_fragment
                  .ConvertChildToLogical(ruby_base_fragment.ScrollableOverflow(
                      NGPhysicalFragment::kEmHeight))
                  .offset.block_offset;
          first_line_top += child.offset.block_offset;
          break;
        }
      }
    }
    ruby_text_box_top = first_line_top - last_line_ruby_text_bottom;
    const LayoutUnit ruby_text_top =
        ruby_text_box_top + ruby_text_box.offset.block_offset;
    if (ruby_text_top < LayoutUnit())
      container_builder_.SetAnnotationOverflow(ruby_text_top);
  } else {
    LayoutUnit first_line_ruby_text_top = ruby_text_box.offset.block_offset;

    // Find a fragment for RubyBase, and get the bottom of text in it.
    LayoutUnit last_line_bottom;
    LayoutUnit base_logical_bottom;
    for (const auto& child : container_builder_.Children()) {
      if (const auto* layout_object = child.fragment->GetLayoutObject()) {
        if (layout_object->IsRubyBase()) {
          LayoutUnit base_block_size =
              child.fragment->Size()
                  .ConvertToLogical(Style().GetWritingMode())
                  .block_size;
          const auto& ruby_base_fragment =
              To<NGPhysicalBoxFragment>(*child.fragment);
          last_line_bottom =
              ruby_base_fragment
                  .ConvertChildToLogical(ruby_base_fragment.ScrollableOverflow(
                      NGPhysicalFragment::kEmHeight))
                  .BlockEndOffset();
          last_line_bottom += child.offset.block_offset;
          base_logical_bottom = child.offset.block_offset + base_block_size;
          break;
        }
      }
    }
    ruby_text_box_top = last_line_bottom - first_line_ruby_text_top;
    LayoutUnit logical_bottom_overflow = ruby_text_box_top +
                                         ruby_text_box.BlockEndOffset() -
                                         base_logical_bottom;
    if (logical_bottom_overflow > LayoutUnit())
      container_builder_.SetAnnotationOverflow(logical_bottom_overflow);
  }
  container_builder_.AddResult(*result,
                               LogicalOffset(LayoutUnit(), ruby_text_box_top));
  // RubyText provides baseline if RubyBase didn't.
  // This behavior doesn't make much sense, but it's compatible with the legacy
  // layout.
  if (!container_builder_.Baseline())
    PropagateBaselineFromChild(ruby_text_fragment, ruby_text_box_top);
}

void NGBlockLayoutAlgorithm::HandleTextControlPlaceholder(
    NGBlockNode placeholder,
    const NGPreviousInflowPosition& previous_inflow_position) {
  DCHECK(Node().IsTextControl()) << Node().GetLayoutBox();

  const bool is_new_fc = placeholder.CreatesNewFormattingContext();
  const NGConstraintSpace space = CreateConstraintSpaceForChild(
      placeholder,
      ComputeChildData(previous_inflow_position, placeholder,
                       /* child_break_token */ nullptr, is_new_fc),
      ChildAvailableSize(), is_new_fc);

  scoped_refptr<const NGLayoutResult> result = placeholder.Layout(space);
  LogicalOffset offset = BorderScrollbarPadding().StartOffset();
  if (Node().IsTextArea()) {
    container_builder_.AddResult(*result, offset);
    return;
  }
  // Another child should provide the baseline.
  DCHECK(container_builder_.Baseline());
  NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                         To<NGPhysicalBoxFragment>(result->PhysicalFragment()));
  // We should apply FirstBaseline() of the placeholder fragment because the
  // placeholder might have the 'overflow' property, and its LastBaseline()
  // might be the block-end margin.
  offset.block_offset =
      *container_builder_.Baseline() - *fragment.FirstBaseline();
  container_builder_.AddResult(*result, offset);

  // This function doesn't update previous_inflow_position. Other children in
  // this container should ignore |placeholder|.
}

LogicalOffset NGBlockLayoutAlgorithm::AdjustSliderThumbInlineOffset(
    const NGFragment& fragment,
    const LogicalOffset& logical_offset) {
  // See LayoutSliderTrack::UpdateLayout().
  const LayoutUnit available_extent =
      ChildAvailableSize().inline_size - fragment.InlineSize();
  const auto* input =
      To<HTMLInputElement>(Node().GetDOMNode()->OwnerShadowHost());
  LayoutUnit offset(input->RatioValue().ToDouble() * available_extent);
  return {logical_offset.inline_offset + offset, logical_offset.block_offset};
}

}  // namespace blink
