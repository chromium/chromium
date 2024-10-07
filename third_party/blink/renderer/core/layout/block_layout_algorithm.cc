// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/block_layout_algorithm.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/block_child_iterator.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/column_spanner_path.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/early_break.h"
#include "third_party/blink/renderer/core/layout/floats_utils.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/inline/ruby_utils.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/list/unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/logical_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/positioned_float.h"
#include "third_party/blink/renderer/core/layout/space_utils.h"
#include "third_party/blink/renderer/core/layout/table/table_layout_utils.h"
#include "third_party/blink/renderer/core/layout/unpositioned_float.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_table_cell_element.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"

namespace blink {
namespace {

bool HasLineEvenIfEmpty(LayoutBox* box) {
  // Note: We should reduce calling |LayoutBlock::HasLineIfEmpty()|, because
  // it calls slow function |IsRootEditableElement()|.
  LayoutBlockFlow* const block_flow = DynamicTo<LayoutBlockFlow>(box);
  if (!block_flow)
    return false;
  // Note: |block_flow->NeedsCollectInline()| is true after removing all
  // children from block[1].
  // [1] editing/inserting/insert_after_delete.html
  if (!GetLayoutObjectForFirstChildNode(block_flow)) {
    // Note: |block_flow->ChildrenInline()| can be both true or false:
    //  - true: just after construction, <div></div>
    //  - true: one of child is inline them remove all, <div>abc</div>
    //  - false: all children are block then remove all, <div><p></p></div>
    return block_flow->HasLineIfEmpty();
  }
  if (AreNGBlockFlowChildrenInline(block_flow)) {
    return block_flow->HasLineIfEmpty() &&
           InlineNode(block_flow).IsBlockLevel();
  }
  if (const auto* const flow_thread = block_flow->MultiColumnFlowThread()) {
    DCHECK(!flow_thread->ChildrenInline());
    for (const auto* child = flow_thread->FirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsInline()) {
        // Note: |LayoutOutsideListMarker| is out-of-flow for the tree
        // building purpose in |LayoutBlockFlow::AddChild()|.
        // |MultiColumnRenderingTest.ListItem| reaches here.
        DCHECK(child->IsLayoutOutsideListMarker()) << child;
        return false;
      }
      if (!child->IsFloatingOrOutOfFlowPositioned()) {
        // We reach here when we have in-flow child.
        // <div style="columns: 3"><div style="float:left"><div></div></div>
        return false;
      }
    }
    // There are no children or all children are floating or out of flow
    // positioned.
    return block_flow->HasLineIfEmpty();
  }
  return false;
}

inline bool IsLastInflowChild(const LayoutBox& box) {
  for (const LayoutObject* next = box.NextSibling(); next;
       next = next->NextSibling()) {
    if (!next->IsFloatingOrOutOfFlowPositioned()) {
      return false;
    }
  }
  return true;
}

inline const LayoutResult* LayoutBlockChild(
    const ConstraintSpace& space,
    const BreakToken* break_token,
    const EarlyBreak* early_break,
    const ColumnSpannerPath* column_spanner_path,
    BlockNode* node) {
  const EarlyBreak* early_break_in_child = nullptr;
  if (early_break) [[unlikely]] {
    early_break_in_child = EnterEarlyBreakInChild(*node, *early_break);
  }
  column_spanner_path = FollowColumnSpannerPath(column_spanner_path, *node);
  return node->Layout(space, To<BlockBreakToken>(break_token),
                      early_break_in_child, column_spanner_path);
}

inline const LayoutResult* LayoutInflow(
    const ConstraintSpace& space,
    const BreakToken* break_token,
    const EarlyBreak* early_break,
    const ColumnSpannerPath* column_spanner_path,
    LayoutInputNode* node,
    InlineChildLayoutContext* context) {
  if (auto* inline_node = DynamicTo<InlineNode>(node)) {
    return inline_node->Layout(space, break_token, column_spanner_path,
                               context);
  }
  return LayoutBlockChild(space, break_token, early_break, column_spanner_path,
                          To<BlockNode>(node));
}

AdjoiningObjectTypes ToAdjoiningObjectTypes(EClear clear) {
  switch (clear) {
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
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
    AdjoiningObjectTypes adjoining_object_types,
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
bool ApplyClearance(const ConstraintSpace& constraint_space,
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

LogicalOffset LogicalFromBfcOffsets(const BfcOffset& child_bfc_offset,
                                    const BfcOffset& parent_bfc_offset,
                                    LayoutUnit child_inline_size,
                                    LayoutUnit parent_inline_size,
                                    TextDirection direction) {
  LayoutUnit inline_offset = LogicalFromBfcLineOffset(
      child_bfc_offset.line_offset, parent_bfc_offset.line_offset,
      child_inline_size, parent_inline_size, direction);

  return {inline_offset,
          child_bfc_offset.block_offset - parent_bfc_offset.block_offset};
}

ItemPosition WebkitTextToItemPosition(ETextAlign text_align) {
  switch (text_align) {
    case ETextAlign::kWebkitLeft:
      return ItemPosition::kLeft;
    case ETextAlign::kWebkitCenter:
      return ItemPosition::kCenter;
    case ETextAlign::kWebkitRight:
      return ItemPosition::kRight;
    default:
      // Ignore non -webkit- values.
      return ItemPosition::kNormal;
  }
}

// Handle text-align:-webkit-* and justify-self.
template <typename ChildInlineSizeFunc>
LayoutUnit WebkitTextAlignAndJustifySelfOffset(
    const ComputedStyle& child_style,
    const ComputedStyle& style,
    LayoutUnit available_space,
    const BoxStrut& margins,
    const ChildInlineSizeFunc& child_inline_size_func) {
  DCHECK(!child_style.MarginInlineStartUsing(style).IsAuto());
  DCHECK(!child_style.MarginInlineEndUsing(style).IsAuto());

  const StyleSelfAlignmentData alignment_data = child_style.ResolvedJustifySelf(
      {ItemPosition::kNormal, OverflowAlignment::kDefault}, &style);
  ItemPosition justify_self = alignment_data.GetPosition();
  OverflowAlignment safe = OverflowAlignment::kSafe;
  if (RuntimeEnabledFeatures::LayoutJustifySelfForBlocksEnabled() &&
      justify_self != ItemPosition::kNormal) {
    safe = alignment_data.Overflow();
  } else {
    justify_self = WebkitTextToItemPosition(style.GetTextAlign());
  }
  auto FreeSpace = [&]() -> LayoutUnit {
    const LayoutUnit free_space =
        available_space - child_inline_size_func() - margins.InlineSum();
    return safe == OverflowAlignment::kSafe ? free_space.ClampNegativeToZero()
                                            : free_space;
  };

  auto self_start_end_converter = [&]() -> LogicalToLogical<LayoutUnit> {
    const LayoutUnit free_space = FreeSpace();
    return LogicalToLogical<LayoutUnit>(
        child_style.GetWritingDirection(), style.GetWritingDirection(),
        /* inline_start */ LayoutUnit(), /* inline_end */ free_space,
        /* block_start */ LayoutUnit(), /* block_end */ free_space);
  };

  bool is_rtl = IsRtl(style.Direction());
  switch (justify_self) {
    case ItemPosition::kLeft:
      return is_rtl ? FreeSpace() : LayoutUnit();
    case ItemPosition::kCenter:
      return FreeSpace() / 2;
    case ItemPosition::kRight:
      return is_rtl ? LayoutUnit() : FreeSpace();
    case ItemPosition::kFlexStart:
    case ItemPosition::kStart:
      return LayoutUnit();
    case ItemPosition::kFlexEnd:
    case ItemPosition::kEnd:
      return FreeSpace();
    case ItemPosition::kSelfStart:
      return self_start_end_converter().InlineStart();
    case ItemPosition::kSelfEnd:
      return self_start_end_converter().InlineEnd();
    default:
      return LayoutUnit();
  }
}

}  // namespace

BlockLayoutAlgorithm::BlockLayoutAlgorithm(const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params),
      previous_result_(params.previous_result),
      column_spanner_path_(params.column_spanner_path),
      line_clamp_data_(params.space.GetLineClampData()),
      fit_all_lines_(false),
      is_resuming_(IsBreakInside(params.break_token)),
      abort_when_bfc_block_offset_updated_(false),
      has_break_opportunity_before_next_child_(false),
      should_text_box_trim_start_(params.space.ShouldTextBoxTrimStart()),
      should_text_box_trim_end_(params.space.ShouldTextBoxTrimEnd()) {
  container_builder_.SetExclusionSpace(params.space.GetExclusionSpace());

  // If this node has a column spanner inside, we'll force it to stay within the
  // current fragmentation flow, so that it doesn't establish a parallel flow,
  // even if it might have content that overflows into the next fragmentainer.
  // This way we'll prevent content that comes after the spanner from being laid
  // out *before* it.
  if (column_spanner_path_)
    container_builder_.SetShouldForceSameFragmentationFlow();

  child_percentage_size_ = CalculateChildPercentageSize(
      GetConstraintSpace(), Node(), ChildAvailableSize());
  replaced_child_percentage_size_ = CalculateReplacedChildPercentageSize(
      GetConstraintSpace(), Node(), ChildAvailableSize(),
      BorderScrollbarPadding(), BorderPadding());

  // If |this| is a list item, keep track of the unpositioned list marker in
  // |container_builder_|.
  if (const BlockNode marker_node = Node().ListMarkerBlockNodeIfListItem()) {
    if (ShouldPlaceUnpositionedListMarker() &&
        !marker_node.ListMarkerOccupiesWholeLine() &&
        (!GetBreakToken() || GetBreakToken()->HasUnpositionedListMarker())) {
      container_builder_.SetUnpositionedListMarker(
          UnpositionedListMarker(marker_node));
    }
  }

  // Initialize `text-box-trim` flags from the `ComputedStyle`.
  const ComputedStyle& style = Node().Style();
  if (style.TextBoxTrim() != ETextBoxTrim::kNone) [[unlikely]] {
    should_text_box_trim_start_ |= style.ShouldTextBoxTrimStart();
    should_text_box_trim_end_ |= style.ShouldTextBoxTrimEnd();
  }
}

// Define the destructor here, so that we can forward-declare more in the
// header.
BlockLayoutAlgorithm::~BlockLayoutAlgorithm() = default;

void BlockLayoutAlgorithm::SetBoxType(PhysicalFragment::BoxType type) {
  container_builder_.SetBoxType(type);
}

MinMaxSizesResult BlockLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput& float_input) {
  if (auto result =
          CalculateMinMaxSizesIgnoringChildren(node_, BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;

  const TextDirection direction = Style().Direction();
  LayoutUnit float_left_inline_size = float_input.float_left_inline_size;
  LayoutUnit float_right_inline_size = float_input.float_right_inline_size;

  for (LayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    // We don't check IsRubyText() here intentionally. RubyText width should
    // affect this width.
    if (child.IsOutOfFlowPositioned() ||
        (child.IsColumnSpanAll() && GetConstraintSpace().IsInColumnBfc())) {
      continue;
    }

    if (child.IsTextControlPlaceholder()) {
      if (Style().ApplyControlFixedSize(Node().GetDOMNode())) {
        continue;
      }
    }

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

    MinMaxSizesFloatInput child_float_input;
    if (child.IsInline() || child.IsAnonymousBlock()) {
      child_float_input.float_left_inline_size = float_left_inline_size;
      child_float_input.float_right_inline_size = float_right_inline_size;
    }

    MinMaxConstraintSpaceBuilder builder(GetConstraintSpace(), Style(), child,
                                         child_is_new_fc);
    builder.SetAvailableBlockSize(ChildAvailableSize().block_size);
    builder.SetPercentageResolutionBlockSize(child_percentage_size_.block_size);
    builder.SetReplacedPercentageResolutionBlockSize(
        replaced_child_percentage_size_.block_size);
    const auto space = builder.ToConstraintSpace();

    MinMaxSizesResult child_result;
    if (child.IsInline()) {
      // From |BlockLayoutAlgorithm| perspective, we can handle |InlineNode|
      // almost the same as |BlockNode|, because an |InlineNode| includes
      // all inline nodes following |child| and their descendants, and produces
      // an anonymous box that contains all line boxes.
      // |NextSibling| returns the next block sibling, or nullptr, skipping all
      // following inline siblings and descendants.
      child_result = To<InlineNode>(child).ComputeMinMaxSizes(
          Style().GetWritingMode(), space, child_float_input);
    } else {
      child_result = ComputeMinAndMaxContentContribution(
          Style(), To<BlockNode>(child), space, child_float_input);
    }
    DCHECK_LE(child_result.sizes.min_size, child_result.sizes.max_size)
        << child.ToString();

    // Determine the max inline contribution of the child.
    BoxStrut margins =
        child.IsInline()
            ? BoxStrut()
            : ComputeMarginsFor(space, child_style, GetConstraintSpace());
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

    depends_on_block_constraints |= child_result.depends_on_block_constraints;

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
  return MinMaxSizesResult(sizes, depends_on_block_constraints);
}

LogicalOffset BlockLayoutAlgorithm::CalculateLogicalOffset(
    const LogicalFragment& fragment,
    LayoutUnit child_bfc_line_offset,
    const std::optional<LayoutUnit>& child_bfc_block_offset) {
  LayoutUnit inline_size = container_builder_.InlineSize();
  TextDirection direction = GetConstraintSpace().Direction();

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

const LayoutResult* BlockLayoutAlgorithm::Layout() {
  const LayoutResult* result = nullptr;
  // Inline children require an inline child layout context to be
  // passed between siblings. We want to stack-allocate that one, but
  // only on demand, as it's quite big.
  InlineNode inline_child(nullptr);
  if (Node().IsInlineFormattingContextRoot(&inline_child)) {
    result = LayoutInlineChild(inline_child);
  } else {
    result = Layout(nullptr);
  }

  if (result->Status() == LayoutResult::kSuccess) {
    return result;
  }

  // To reduce stack usage, handle non-successful results in a separate
  // function.
  return HandleNonsuccessfulLayoutResult(result);
}

NOINLINE const LayoutResult*
BlockLayoutAlgorithm::HandleNonsuccessfulLayoutResult(
    const LayoutResult* result) {
  DCHECK_NE(result->Status(), LayoutResult::kSuccess);
  switch (result->Status()) {
    case LayoutResult::kNeedsEarlierBreak: {
      // If we found a good break somewhere inside this block, re-layout and
      // break at that location.
      DCHECK(result->GetEarlyBreak());

      LayoutAlgorithmParams params(
          Node(), container_builder_.InitialFragmentGeometry(),
          GetConstraintSpace(), GetBreakToken(), result->GetEarlyBreak());
      params.column_spanner_path = column_spanner_path_;
      BlockLayoutAlgorithm algorithm_with_break(params);
      return RelayoutAndBreakEarlier(&algorithm_with_break);
    }
    case LayoutResult::kNeedsLineClampRelayout:
      if (line_clamp_data_.data.state == LineClampData::kClampByLines) {
        return RelayoutIgnoringLineClamp();
      }
      if (GetConstraintSpace().IsNewFormattingContext()) {
        return RelayoutWithLineClampBlockSize(result->LinesUntilClamp());
      }
      // Propagate the error upwards until we reach the BFC root.
      return result;
    case LayoutResult::kDisableFragmentation:
      DCHECK(GetConstraintSpace().HasBlockFragmentation());
      return RelayoutWithoutFragmentation<BlockLayoutAlgorithm>();
    case LayoutResult::kTextBoxTrimEndDidNotApply:
      return RelayoutForTextBoxTrimEnd();
    default:
      return result;
  }
}

NOINLINE const LayoutResult* BlockLayoutAlgorithm::LayoutInlineChild(
    const InlineNode& node) {
  const TextWrapStyle wrap = node.Style().GetTextWrapStyle();
  if (wrap == TextWrapStyle::kPretty) [[unlikely]] {
    UseCounter::Count(node.GetDocument(), WebFeature::kTextWrapPretty);
    if (!node.IsScoreLineBreakDisabled()) {
      return LayoutWithOptimalInlineChildLayoutContext<kMaxLinesForOptimal>(
          node);
    }
  } else if (wrap == TextWrapStyle::kBalance) [[unlikely]] {
    UseCounter::Count(node.GetDocument(), WebFeature::kTextWrapBalance);
    if (!node.IsScoreLineBreakDisabled()) {
      return LayoutWithOptimalInlineChildLayoutContext<kMaxLinesForBalance>(
          node);
    }
  } else {
    DCHECK(ShouldWrapLineGreedy(wrap));
  }

  SimpleInlineChildLayoutContext context(node, &container_builder_);
  return Layout(&context);
}

template <wtf_size_t capacity>
NOINLINE const LayoutResult*
BlockLayoutAlgorithm::LayoutWithOptimalInlineChildLayoutContext(
    const InlineNode& child) {
  OptimalInlineChildLayoutContext<capacity> context(child, &container_builder_);
  const LayoutResult* result = Layout(&context);
  return result;
}

NOINLINE const LayoutResult* BlockLayoutAlgorithm::RelayoutIgnoringLineClamp() {
  DCHECK_EQ(line_clamp_data_.data.state, LineClampData::kClampByLines);
  LayoutAlgorithmParams params(Node(),
                               container_builder_.InitialFragmentGeometry(),
                               GetConstraintSpace(), GetBreakToken(), nullptr);
  BlockLayoutAlgorithm algorithm_ignoring_line_clamp(params);
  algorithm_ignoring_line_clamp.line_clamp_data_.data.state =
      LineClampData::kDontTruncate;
  BoxFragmentBuilder& new_builder =
      algorithm_ignoring_line_clamp.container_builder_;
  new_builder.SetBoxType(container_builder_.GetBoxType());
  return algorithm_ignoring_line_clamp.Layout();
}

NOINLINE const LayoutResult*
BlockLayoutAlgorithm::RelayoutWithLineClampBlockSize(int lines_until_clamp) {
  DCHECK_EQ(line_clamp_data_.data.state,
            LineClampData::kMeasureLinesUntilBfcOffset);
  LayoutAlgorithmParams params(Node(),
                               container_builder_.InitialFragmentGeometry(),
                               GetConstraintSpace(), GetBreakToken(), nullptr);
  BlockLayoutAlgorithm algorithm_ignoring_line_clamp(params);
  algorithm_ignoring_line_clamp.line_clamp_data_.data.state =
      LineClampData::kClampByLines;
  algorithm_ignoring_line_clamp.line_clamp_data_.data.lines_until_clamp =
      std::max(1, lines_until_clamp);
  algorithm_ignoring_line_clamp.line_clamp_data_.end_margin_strut =
      line_clamp_data_.end_margin_strut;
  BoxFragmentBuilder& new_builder =
      algorithm_ignoring_line_clamp.container_builder_;
  new_builder.SetBoxType(container_builder_.GetBoxType());
  return algorithm_ignoring_line_clamp.Layout();
}

// Re-layout when the `child` failed to apply `text-box-trim: end`.
NOINLINE const LayoutResult* BlockLayoutAlgorithm::RelayoutForTextBoxTrimEnd() {
  if (last_non_empty_inflow_child_) {
    // If there is at least one non-empty inflow child, re-layout by applying
    // the `text-box-trim: end` to the `last_non_empty_inflow_child_`.
    LayoutAlgorithmParams params{Node(),
                                 container_builder_.InitialFragmentGeometry(),
                                 GetConstraintSpace(), GetBreakToken()};
    BlockLayoutAlgorithm relayout_algorithm{params};
    relayout_algorithm.override_text_box_trim_end_child_ =
        last_non_empty_inflow_child_;
    relayout_algorithm.override_text_box_trim_end_break_token_ =
        last_non_empty_break_token_;
    BoxFragmentBuilder& new_builder = relayout_algorithm.container_builder_;
    new_builder.SetBoxType(container_builder_.GetBoxType());
    return relayout_algorithm.Layout();
  }

  if (!GetConstraintSpace().ShouldTextBoxTrimEnd()) {
    // If there are no more ancestors to propagate, re-layout by ignoring the
    // `text-box-trim: end`.
    LayoutAlgorithmParams params{Node(),
                                 container_builder_.InitialFragmentGeometry(),
                                 GetConstraintSpace(), GetBreakToken()};
    BlockLayoutAlgorithm relayout_algorithm{params};
    relayout_algorithm.should_text_box_trim_end_ = false;
    BoxFragmentBuilder& new_builder = relayout_algorithm.container_builder_;
    new_builder.SetBoxType(container_builder_.GetBoxType());
    return relayout_algorithm.Layout();
  }

  return container_builder_.Abort(LayoutResult::kTextBoxTrimEndDidNotApply);
}

inline const LayoutResult* BlockLayoutAlgorithm::Layout(
    InlineChildLayoutContext* inline_child_layout_context) {
  DCHECK_EQ(!!inline_child_layout_context,
            Node().IsInlineFormattingContextRoot());
  container_builder_.SetIsInlineFormattingContext(inline_child_layout_context);

  const auto& constraint_space = GetConstraintSpace();
  container_builder_.SetBfcLineOffset(
      constraint_space.GetBfcOffset().line_offset);

  if (auto adjoining_object_types =
          constraint_space.GetAdjoiningObjectTypes()) {
    DCHECK(!constraint_space.IsNewFormattingContext());
    DCHECK(!container_builder_.BfcBlockOffset());

    // If there were preceding adjoining objects, they will be affected when the
    // BFC block-offset gets resolved or updated. We then need to roll back and
    // re-layout those objects with the new BFC block-offset, once the BFC
    // block-offset is updated.
    abort_when_bfc_block_offset_updated_ = true;

    container_builder_.SetAdjoiningObjectTypes(adjoining_object_types);
  } else if (constraint_space.HasBlockFragmentation()) {
    // The offset from the block-start of the fragmentainer is part of the
    // constraint space, so if this offset changes, we need to abort.
    abort_when_bfc_block_offset_updated_ = true;
  }

  if (Style().HasAutoStandardLineClamp()) {
    if (!line_clamp_data_.data.IsLineClampContext()) {
      LayoutUnit clamp_bfc_offset = ChildAvailableSize().block_size;
      if (clamp_bfc_offset == kIndefiniteSize) {
        const MinMaxSizes sizes = ComputeInitialMinMaxBlockSizes(
            constraint_space, Node(), BorderPadding());
        if (sizes.max_size != LayoutUnit::Max()) {
          clamp_bfc_offset =
              (sizes.max_size - BorderScrollbarPadding().block_end)
                  .ClampNegativeToZero();
        }
      } else {
        clamp_bfc_offset =
            (BorderScrollbarPadding().block_start + clamp_bfc_offset)
                .ClampNegativeToZero();
      }
      line_clamp_data_.UpdateClampOffsetFromStyle(
          clamp_bfc_offset, BorderScrollbarPadding().block_start);
    }
  } else if (Style().HasLineClamp()) {
    if (!line_clamp_data_.data.IsLineClampContext()) {
      line_clamp_data_.UpdateLinesFromStyle(Style().LineClamp());
    }
  } else {
    if (Style().WebkitLineClamp() != 0) {
      UseCounter::Count(Node().GetDocument(),
                        WebFeature::kWebkitLineClampWithoutWebkitBox);
    }

    // If we're clamping by BFC offset, we need to subtract the bottom bmp to
    // leave room for it. This doesn't apply if we're relaying out to fix the
    // offset, because that already accounts for the bmp.
    if (line_clamp_data_.data.state ==
        LineClampData::kMeasureLinesUntilBfcOffset) {
      MarginStrut end_margin_strut = constraint_space.LineClampEndMarginStrut();
      end_margin_strut.Append(
          ComputeMarginsForSelf(constraint_space, Style()).block_end,
          /* is_quirky */ false);

      // `constraint_space.LineClampEndMarginStrut().Sum()` is the margin
      // contribution from our ancestor boxes, which has already been taken
      // into account for the clamp BFC offset that we have. We only need to
      // add any additional margin contribution from this box's margin.
      line_clamp_data_.data.clamp_bfc_offset -=
          BorderScrollbarPadding().block_end +
          (end_margin_strut.Sum() -
           constraint_space.LineClampEndMarginStrut().Sum());

      // The presence of borders and padding blocks margin propagation.
      if (!BorderScrollbarPadding().block_end) {
        line_clamp_data_.end_margin_strut = end_margin_strut;
      }
    }
  }

  LayoutUnit content_edge = BorderScrollbarPadding().block_start;

  PreviousInflowPosition previous_inflow_position = {
      LayoutUnit(), constraint_space.GetMarginStrut(),
      is_resuming_ ? LayoutUnit() : container_builder_.Padding().block_start,
      /* self_collapsing_child_had_clearance */ false};

  if (GetBreakToken()) {
    if (IsBreakInside(GetBreakToken()) && !GetBreakToken()->IsForcedBreak() &&
        !GetBreakToken()->IsCausedByColumnSpanner()) {
      // If the block container is being resumed after an unforced break,
      // margins inside may be adjoining with the fragmentainer boundary.
      previous_inflow_position.margin_strut.discard_margins = true;
    }

    if (GetBreakToken()->MonolithicOverflow()) {
      // If we have been pushed by monolithic overflow that started on a
      // previous page, we'll behave as if there's a valid breakpoint before the
      // first child here, and that it has perfect break appeal. This isn't
      // always strictly correct (the monolithic content in question may have
      // break-after:avoid, for instance), but should be a reasonable approach,
      // unless we want to make a bigger effort.
      has_break_opportunity_before_next_child_ = true;
    }
  }

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
      constraint_space.IsNewFormattingContext()) {
    bool discard_subsequent_margins =
        previous_inflow_position.margin_strut.discard_margins && !content_edge;
    if (!ResolveBfcBlockOffset(&previous_inflow_position)) {
      // There should be no preceding content that depends on the BFC block
      // offset of a new formatting context block, and likewise when resuming
      // from a break token.
      DCHECK(!constraint_space.IsNewFormattingContext());
      DCHECK(!is_resuming_);
      return container_builder_.Abort(LayoutResult::kBfcBlockOffsetResolved);
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
  if (constraint_space.IsNewFormattingContext()) {
    DCHECK_EQ(*container_builder_.BfcBlockOffset(), LayoutUnit());
  }
  // If this is a new formatting context, or if we're resuming from a break
  // token, no margin strut must be lingering around at this point.
  if (constraint_space.IsNewFormattingContext() || is_resuming_) {
    DCHECK(constraint_space.GetMarginStrut().IsEmpty());
  }

  if (!container_builder_.BfcBlockOffset()) {
    // New formatting-contexts, and when we have a self-collapsing child
    // affected by clearance must already have their BFC block-offset resolved.
    DCHECK(!previous_inflow_position.self_collapsing_child_had_clearance);
    DCHECK(!constraint_space.IsNewFormattingContext());
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
  const InlineBreakToken* previous_inline_break_token = nullptr;

  BlockChildIterator child_iterator(Node().FirstChild(), GetBreakToken());

  // If this layout is blocked by a display-lock, then we pretend this node has
  // no children and that there are no break tokens. Due to this, we skip layout
  // on these children.
  if (Node().ChildLayoutBlockedByDisplayLock())
    child_iterator = BlockChildIterator(BlockNode(nullptr), nullptr);

  BlockNode ruby_text_child(nullptr);
  BlockNode placeholder_child(nullptr);
  BlockChildIterator::Entry entry;
  for (entry = child_iterator.NextChild(); LayoutInputNode child = entry.node;
       entry = child_iterator.NextChild(previous_inline_break_token)) {
    const BreakToken* child_break_token = entry.token;

    if (child.IsOutOfFlowPositioned()) {
      // Out-of-flow fragmentation is a special step that takes place after
      // regular layout, so we should never resume anything here. However, we
      // may have break-before tokens, when a column spanner is directly
      // followed by an OOF.
      DCHECK(!child_break_token ||
             (child_break_token->IsBlockType() &&
              To<BlockBreakToken>(child_break_token)->IsBreakBefore()));
      HandleOutOfFlowPositioned(previous_inflow_position, To<BlockNode>(child));
    } else if (child.IsFloating()) {
      HandleFloat(previous_inflow_position, To<BlockNode>(child),
                  To<BlockBreakToken>(child_break_token));
    } else if (child.IsListMarker() && !child.ListMarkerOccupiesWholeLine()) {
      // Ignore outside list markers because they are already set to
      // |container_builder_.UnpositionedListMarker| in the constructor, unless
      // |ListMarkerOccupiesWholeLine|, which is handled like a regular child.
    } else if (child.IsColumnSpanAll() && constraint_space.IsInColumnBfc() &&
               constraint_space.HasBlockFragmentation()) {
      // The child is a column spanner. If we have no breaks inside (in parallel
      // flows), we now need to finish this fragmentainer, then abort and let
      // the column layout algorithm handle the spanner as a child. The
      // HasBlockFragmentation() check above may seem redundant, but this is
      // important if we're overflowing a clipped container. In such cases, we
      // won't treat the spanner as one, since we shouldn't insert any breaks in
      // that mode.
      DCHECK(!container_builder_.DidBreakSelf());
      DCHECK(!container_builder_.FoundColumnSpanner());
      DCHECK(!IsBreakInside(To<BlockBreakToken>(child_break_token)));

      if (constraint_space.IsPastBreak() ||
          container_builder_.HasInsertedChildBreak()) {
        // Something broke inside (typically in a parallel flow, or we wouldn't
        // be here). Before we can handle the spanner, we need to finish what
        // comes before it.
        container_builder_.AddBreakBeforeChild(child, kBreakAppealPerfect,
                                               /* is_forced_break */ true);

        // We're not ready to go back and lay out the spanner yet (see above),
        // so we don't set a spanner path, but since we did find a spanner, make
        // a note of it. This will make sure that we resolve our BFC block-
        // offset, so that we don't incorrectly appear to be self-collapsing.
        container_builder_.SetHasColumnSpanner(true);
        break;
      }

      // Establish a column spanner path. The innermost node will be the spanner
      // itself, wrapped inside the container handled by this layout algorithm.
      const auto* child_spanner_path =
          MakeGarbageCollected<ColumnSpannerPath>(To<BlockNode>(child));
      const auto* container_spanner_path =
          MakeGarbageCollected<ColumnSpannerPath>(Node(), child_spanner_path);
      container_builder_.SetColumnSpannerPath(container_spanner_path);

      // In order to properly collapse column spanner margins, we need to know
      // if the column spanner's parent was empty, for example, in the case that
      // the only child content of the parent since the last spanner is an OOF
      // that will get positioned outside the multicol.
      container_builder_.SetIsEmptySpannerParent(
          container_builder_.Children().empty() && is_resuming_);
      // After the spanner(s), we are going to resume inside this block. If
      // there's a subsequent sibling that's not a spanner, we're resume right
      // in front of that one. Otherwise we'll just resume after all the
      // children.
      for (entry = child_iterator.NextChild();
           LayoutInputNode sibling = entry.node;
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
      ruby_text_child = To<BlockNode>(child);
    } else if (child.IsTextControlPlaceholder()) {
      placeholder_child = To<BlockNode>(child);
    } else {
      // If this is the child we had previously determined to break before, do
      // so now and finish layout.
      if (early_break_ && IsEarlyBreakTarget(*early_break_, container_builder_,
                                             child)) [[unlikely]] {
        if (!ResolveBfcBlockOffset(&previous_inflow_position)) {
          // However, the predetermined breakpoint may be exactly where the BFC
          // block-offset gets resolved. If that hasn't yet happened, we need to
          // do that first and re-layout at the right BFC block-offset, and THEN
          // break.
          return container_builder_.Abort(
              LayoutResult::kBfcBlockOffsetResolved);
        }
        container_builder_.AddBreakBeforeChild(child, kBreakAppealPerfect,
                                               /* is_forced_break */ false);
        ConsumeRemainingFragmentainerSpace(&previous_inflow_position);
        break;
      }

      LayoutResult::EStatus status;
      if (child.CreatesNewFormattingContext()) {
        status = HandleNewFormattingContext(
            child, To<BlockBreakToken>(child_break_token),
            &previous_inflow_position);
        previous_inline_break_token = nullptr;
      } else {
        status = HandleInflow(
            child, child_break_token, &previous_inflow_position,
            inline_child_layout_context, &previous_inline_break_token);
      }

      if (status != LayoutResult::kSuccess) {
        // We need to abort the layout. No fragment will be generated.
        return container_builder_.Abort(status);
      }
      if (constraint_space.HasBlockFragmentation()) {
        // A child break in a parallel flow doesn't affect whether we should
        // break here or not.
        if (container_builder_.HasInflowChildBreakInside()) {
          // But if the break happened in the same flow, we'll now just finish
          // layout of the fragment. No more siblings should be processed.
          break;
        }
      }
    }
  }

#if DCHECK_IS_ON()
  // Assert that we have made actual progress. Breaking before we're done with
  // all parallel flows from incoming break tokens means that we'll never get
  // the opportunity to handle them again. We don't repropagate unhandled
  // incoming break tokens, and there should be no need to.
  if (auto* inline_token = DynamicTo<InlineBreakToken>(entry.token)) {
    DCHECK(!inline_token->IsInParallelBlockFlow());
  } else if (auto* block_token = DynamicTo<BlockBreakToken>(entry.token)) {
    // A column spanner forces all content preceding it to stay in the same
    // flow, so we can (and must) skip the check. Even if IsAtBlockEnd() is true
    // in such cases, it doesn't mean that a parallel flow is established.
    if (!container_builder_.FoundColumnSpanner() &&
        !container_builder_.ShouldForceSameFragmentationFlow()) {
      DCHECK(!block_token->IsAtBlockEnd());
    }
  }
#endif

  if (ruby_text_child)
    HandleRubyText(ruby_text_child);
  if (placeholder_child) {
    previous_inflow_position.logical_block_offset =
        HandleTextControlPlaceholder(placeholder_child,
                                     previous_inflow_position);
  }

  if (constraint_space.IsNewFormattingContext() &&
      line_clamp_data_.ShouldRelayoutWithNoForcedTruncate()) [[unlikely]] {
    // Truncation of the last line was forced, but there are no lines after the
    // truncated line. Rerun layout without forcing truncation. This is only
    // done if line-clamp was specified on the element as the element containing
    // the node may have subsequent lines. If there aren't, the containing
    // element will relayout.
    return container_builder_.Abort(LayoutResult::kNeedsLineClampRelayout);
  }

  if (constraint_space.ShouldTextBoxTrimEnd() &&
      !container_builder_.IsBlockEndTrimmed()) [[unlikely]] {
    // The `text-box-trim: end` should apply to the last inflow child. If that
    // turned out to be empty, it should be applied to the previous child
    // instead.
    return container_builder_.Abort(LayoutResult::kTextBoxTrimEndDidNotApply);
  }

  if (!child_iterator.NextChild(previous_inline_break_token).node) {
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

const LayoutResult* BlockLayoutAlgorithm::FinishLayout(
    PreviousInflowPosition* previous_inflow_position,
    InlineChildLayoutContext* inline_child_layout_context) {
  // With CSSLineClamp enabled, if we line-clamped inside this box, its size
  // must be set exactly as if there were no layout boxes after the clamp point.
  // We therefore use the previous inflow position that we saved at the clamp
  // point.
  if (RuntimeEnabledFeatures::CSSLineClampEnabled() &&
      line_clamp_data_.previous_inflow_position_when_clamped.has_value())
      [[unlikely]] {
    previous_inflow_position =
        &*line_clamp_data_.previous_inflow_position_when_clamped;
  }

  const auto& constraint_space = GetConstraintSpace();
  LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();
  MarginStrut end_margin_strut = previous_inflow_position->margin_strut;

  // Add line height for empty content editable or button with empty label, e.g.
  // <div contenteditable></div>, <input type="button" value="">
  if (container_builder_.HasSeenAllChildren() &&
      HasLineEvenIfEmpty(Node().GetLayoutBox())) {
    intrinsic_block_size_ = std::max(
        intrinsic_block_size_, BorderScrollbarPadding().block_start +
                                   Node().EmptyLineBlockSize(GetBreakToken()));
    if (container_builder_.IsInitialColumnBalancingPass()) {
      container_builder_.PropagateTallestUnbreakableBlockSize(
          intrinsic_block_size_);
    }
    // Test [1][2] require baseline offset for empty editable.
    // [1] css3/flexbox/baseline-for-empty-line.html
    // [2] inline-block/contenteditable-baseline.html
    const LayoutBlock* const layout_block =
        To<LayoutBlock>(Node().GetLayoutBox());
    if (auto baseline_offset = layout_block->BaselineForEmptyLine()) {
      container_builder_.SetBaselines(*baseline_offset);
    }
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

  // If line clamping occurred, and we're using the legacy behavior, the
  // intrinsic block-size comes from the intrinsic block-size at the time of the
  // clamp, without taking margins, clearance, etc. into account.
  if (!RuntimeEnabledFeatures::CSSLineClampEnabled() &&
      line_clamp_data_.previous_inflow_position_when_clamped) {
    DCHECK(container_builder_.BfcBlockOffset());
    intrinsic_block_size_ =
        line_clamp_data_.previous_inflow_position_when_clamped
            ->logical_block_offset +
        BorderScrollbarPadding().block_end;
    end_margin_strut = MarginStrut();
  } else if (BorderScrollbarPadding().block_end ||
             previous_inflow_position->self_collapsing_child_had_clearance ||
             constraint_space.IsNewFormattingContext()) {
    // The end margin strut of an in-flow fragment contributes to the size of
    // the current fragment if:
    //  - There is block-end border/scrollbar/padding.
    //  - There was a self-collapsing child affected by clearance.
    //  - We are a new formatting context.
    // Additionally this fragment produces no end margin strut.

    // If the current layout is a new formatting context, we need to encapsulate
    // all of our floats, except for those that were hidden because of
    // line-clamp.
    if (constraint_space.IsNewFormattingContext()) {
      LayoutUnit clearance =
          GetExclusionSpace().NonHiddenClearanceOffsetIncludingInitialLetter();
#ifdef DCHECK_ALWAYS_ON
      if (!RuntimeEnabledFeatures::CSSLineClampEnabled() ||
          !line_clamp_data_.previous_inflow_position_when_clamped) {
        DCHECK_EQ(clearance,
                  GetExclusionSpace().ClearanceOffsetIncludingInitialLetter(
                      EClear::kBoth));
      }
#endif
      intrinsic_block_size_ = std::max(intrinsic_block_size_, clearance);
    }

    if (!container_builder_.BfcBlockOffset()) {
      // If we have collapsed through the block start and all children (if any),
      // now is the time to determine the BFC block offset, because finally we
      // have found something solid to hang on to (like clearance or a bottom
      // border, for instance). If we're a new formatting context, though, we
      // shouldn't be here, because then the offset should already have been
      // determined.
      DCHECK(!constraint_space.IsNewFormattingContext());
      if (!ResolveBfcBlockOffset(previous_inflow_position)) {
        return container_builder_.Abort(LayoutResult::kBfcBlockOffsetResolved);
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

      if (constraint_space.HasKnownFragmentainerBlockSize()) {
        LayoutUnit new_margin_strut_sum = AdjustedMarginAfterFinalChildFragment(
            container_builder_, previous_inflow_position->logical_block_offset,
            margin_strut_sum);
        if (new_margin_strut_sum != margin_strut_sum) {
          container_builder_.SetIsTruncatedByFragmentationLine();
          margin_strut_sum = new_margin_strut_sum;
        }
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

    if (!ShouldIncludeBlockEndBorderPadding(container_builder_)) {
      // The block-end edge isn't in this fragment. We either haven't got there
      // yet, or we're past it (and are overflowing). So don't add trailing
      // border/padding.
      container_builder_.ClearBorderScrollbarPaddingBlockEnd();
    }
    intrinsic_block_size_ += BorderScrollbarPadding().block_end;
    end_margin_strut = MarginStrut();
  } else {
    // Update our intrinsic block size to be just past the block-end border edge
    // of the last in-flow child. The pending margin is to be propagated to our
    // container, so ignore it.
    intrinsic_block_size_ = std::max(
        intrinsic_block_size_, previous_inflow_position->logical_block_offset);
  }

  LayoutUnit unconstrained_intrinsic_block_size = intrinsic_block_size_;
  intrinsic_block_size_ = ClampIntrinsicBlockSize(
      constraint_space, Node(), GetBreakToken(), BorderScrollbarPadding(),
      intrinsic_block_size_,
      CalculateQuirkyBodyMarginBlockSum(end_margin_strut));

  // In order to calculate the block-size for the fragment, we need to compare
  // the combined intrinsic block-size of all fragments to e.g. specified
  // block-size. We'll skip this part if this is a fragmentainer.
  // Fragmentainers never have a specified block-size anyway, but, more
  // importantly, adding consumed block-size, and then subtracting it again
  // later (when setting the final fragment size) would produce incorrect
  // results if the sum becomes "infinity", i.e. LayoutUnit::Max(). Skipping
  // this will allow the total block-size of all the fragmentainers to become
  // greater than LayoutUnit::Max(). This is important for column balancing, or
  // we'd fail to finish very tall child content properly, ending up with too
  // many fragmentainers, since the fragmentainers produced would be too short
  // to fit as much as necessary. Basically: don't mess up (clamp) the measument
  // we've already done.
  LayoutUnit previously_consumed_block_size;
  if (GetBreakToken() && !container_builder_.IsFragmentainerBoxType())
      [[unlikely]] {
    previously_consumed_block_size = GetBreakToken()->ConsumedBlockSize();
  }

  // Recompute the block-axis size now that we know our content size.
  border_box_size.block_size = ComputeBlockSizeForFragment(
      constraint_space, Node(), BorderPadding(),
      previously_consumed_block_size + intrinsic_block_size_,
      border_box_size.inline_size);
  container_builder_.SetFragmentsTotalBlockSize(border_box_size.block_size);

  // If our BFC block-offset is still unknown, we check:
  //  - If we have a non-zero block-size (margins don't collapse through us).
  //  - If we have a break token. (Even if we are self-collapsing we position
  //    ourselves at the very start of the fragmentainer).
  //  - We got interrupted by a column spanner.
  if (!container_builder_.BfcBlockOffset() &&
      (border_box_size.block_size || GetBreakToken() ||
       container_builder_.FoundColumnSpanner())) {
    if (!ResolveBfcBlockOffset(previous_inflow_position))
      return container_builder_.Abort(LayoutResult::kBfcBlockOffsetResolved);
    DCHECK(container_builder_.BfcBlockOffset());
  }

  if (container_builder_.BfcBlockOffset()) {
    // Do not collapse margins between the last in-flow child and bottom margin
    // of its parent if:
    //  - The block-size differs from the intrinsic size.
    //  - The parent has a definite initial block-size.
    const LayoutUnit initial_block_size = ComputeInitialBlockSizeForFragment(
        constraint_space, Node(), BorderPadding(), kIndefiniteSize,
        border_box_size.inline_size);
    if (border_box_size.block_size != intrinsic_block_size_ ||
        initial_block_size != kIndefiniteSize) {
      end_margin_strut = MarginStrut();
    }
  }

  // List markers should have been positioned if we had line boxes, or boxes
  // that have line boxes. If there were no line boxes, position without line
  // boxes.
  if (container_builder_.GetUnpositionedListMarker() &&
      ShouldPlaceUnpositionedListMarker() &&
      // If the list-item is block-fragmented, leave it unpositioned and expect
      // following fragments have a line box.
      !container_builder_.HasInflowChildBreakInside()) {
    if (!PositionListMarkerWithoutLineBoxes(previous_inflow_position))
      return container_builder_.Abort(LayoutResult::kBfcBlockOffsetResolved);
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
    if (constraint_space.ForcedBfcBlockOffset()) {
      container_builder_.SetBfcBlockOffset(
          *constraint_space.ForcedBfcBlockOffset());

      // Also make sure that this is treated as a valid class C breakpoint (if
      // it is one).
      if (constraint_space.IsPushedByFloats()) {
        container_builder_.SetIsPushedByFloats();
      }
    }
  }

  if (InvolvedInBlockFragmentation(container_builder_)) [[unlikely]] {
    BreakStatus status = FinalizeForFragmentation();
    if (status != BreakStatus::kContinue) {
      if (status == BreakStatus::kNeedsEarlierBreak) {
        return container_builder_.Abort(LayoutResult::kNeedsEarlierBreak);
      }
      DCHECK_EQ(status, BreakStatus::kDisableFragmentation);
      return container_builder_.Abort(LayoutResult::kDisableFragmentation);
    }

    // Read the intrinsic block-size back, since it may have been reduced due to
    // fragmentation.
    intrinsic_block_size_ = container_builder_.IntrinsicBlockSize();
  } else {
#if DCHECK_IS_ON()
  // If we're not participating in a fragmentation context, no block
  // fragmentation related fields should have been set.
  container_builder_.CheckNoBlockFragmentation();
#endif
  }

  // At this point, perform any final table-cell adjustments needed.
  if (constraint_space.IsTableCell()) {
    FinalizeTableCellLayout(intrinsic_block_size_, &container_builder_);
  } else {
    AlignBlockContent(Style(), GetBreakToken(),
                      unconstrained_intrinsic_block_size, container_builder_);
  }

  container_builder_.HandleOofsAndSpecialDescendants();

  if (constraint_space.GetBaselineAlgorithmType() ==
      BaselineAlgorithmType::kInlineBlock) {
    container_builder_.SetUseLastBaselineForInlineBaseline();
  }

  // An exclusion space is confined to nodes within the same formatting context.
  if (constraint_space.IsNewFormattingContext()) {
    container_builder_.SetExclusionSpace(ExclusionSpace());
  } else {
    container_builder_.SetLinesUntilClamp(
        line_clamp_data_.data.LinesUntilClamp(/*show_measured_lines*/ true));
  }

  if (constraint_space.UseFirstLineStyle()) {
    container_builder_.SetStyleVariant(StyleVariant::kFirstLine);
  }

  return container_builder_.ToBoxFragment();
}

bool BlockLayoutAlgorithm::TryReuseFragmentsFromCache(
    InlineNode inline_node,
    PreviousInflowPosition* previous_inflow_position,
    const InlineBreakToken** inline_break_token_out) {
  DCHECK(previous_result_);

  // No lines are reusable if this block uses paragraph-level line breakers such
  // as `ParagraphLineBreaker` or `ScoreLineBreaker`.
  if (!Style().ShouldWrapLineGreedy()) {
    return false;
  }

  const auto& previous_fragment =
      To<PhysicalBoxFragment>(previous_result_->GetPhysicalFragment());
  const FragmentItems* previous_items = previous_fragment.Items();
  DCHECK(previous_items);

  // Find reusable lines. Fail if no items are reusable.
  // TODO(kojii): |DirtyLinesFromNeedsLayout| is needed only once for a
  // |LayoutBlockFlow|, not for every fragment.
  FragmentItems::DirtyLinesFromNeedsLayout(*inline_node.GetLayoutBlockFlow());
  const FragmentItem* end_item =
      previous_items->EndOfReusableItems(previous_fragment);
  DCHECK(end_item);
  if (!end_item || end_item == &previous_items->front())
    return false;

  wtf_size_t max_lines = 0;
  if (std::optional<int> lines_until_clamp =
          line_clamp_data_.LinesUntilClamp()) {
    // There is an additional logic for the last clamped line. Reuse only up to
    // before that to use the same logic.
    if (*lines_until_clamp <= 1) {
      return false;
    }
    max_lines = *lines_until_clamp - 1;
  }

  const auto& children = container_builder_.Children();
  const wtf_size_t children_before = children.size();
  FragmentItemsBuilder* items_builder = container_builder_.ItemsBuilder();
  const auto& space = GetConstraintSpace();
  DCHECK_EQ(items_builder->GetWritingDirection(), space.GetWritingDirection());
  const auto result =
      items_builder->AddPreviousItems(previous_fragment, *previous_items,
                                      &container_builder_, end_item, max_lines);
  if (!result.succeeded) [[unlikely]] {
    DCHECK_EQ(children.size(), children_before);
    DCHECK(!result.used_block_size);
    DCHECK(!result.inline_break_token);
    return false;
  }

  // To reach here we mustn't have any adjoining objects, and the first line
  // must have content. Resolving the BFC block-offset here should never fail.
  DCHECK(!abort_when_bfc_block_offset_updated_);
  bool success = ResolveBfcBlockOffset(previous_inflow_position);
  DCHECK(success);
  DCHECK(container_builder_.BfcBlockOffset());

  DCHECK_GT(result.line_count, 0u);
  if (max_lines) {
    DCHECK(result.line_count <= max_lines);
    DCHECK_EQ(line_clamp_data_.data.state, LineClampData::kClampByLines);
    line_clamp_data_.data.lines_until_clamp -= result.line_count;
  } else if (line_clamp_data_.data.state ==
             LineClampData::kMeasureLinesUntilBfcOffset) {
    line_clamp_data_.data.lines_until_clamp += result.line_count;
  }

  // |AddPreviousItems| may have added more than one lines. Propagate baselines
  // from them.
  for (const auto& child : base::make_span(children).subspan(children_before)) {
    DCHECK(child.fragment->IsLineBox());
    PropagateBaselineFromLineBox(*child.fragment, child.offset.block_offset);
  }

  previous_inflow_position->logical_block_offset += result.used_block_size;
  *inline_break_token_out = result.inline_break_token;
  return true;
}

void BlockLayoutAlgorithm::HandleOutOfFlowPositioned(
    const PreviousInflowPosition& previous_inflow_position,
    BlockNode child) {
  if (GetConstraintSpace().HasBlockFragmentation()) {
    // Forced breaks cannot be specified directly on out-of-flow positioned
    // elements, but if the preceding block has a forced break after, we need to
    // break before it. Note that we really only need to do this if block-start
    // offset is auto (but it's harmless to do it also when it's non-auto).
    EBreakBetween break_between =
        container_builder_.JoinedBreakBetweenValue(EBreakBetween::kAuto);
    if (IsForcedBreakValue(GetConstraintSpace(), break_between)) {
      container_builder_.AddBreakBeforeChild(child, kBreakAppealPerfect,
                                             /* is_forced_break*/ true);
      return;
    }
  }

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
            GetConstraintSpace().ExpectedBfcBlockOffset()) +
        static_offset.block_offset;

    BfcOffset origin_bfc_offset = {
        GetConstraintSpace().GetBfcOffset().line_offset +
            BorderScrollbarPadding().LineLeft(Style().Direction()),
        origin_bfc_block_offset};

    static_offset.inline_offset += CalculateOutOfFlowStaticInlineLevelOffset(
        Style(), origin_bfc_offset, GetExclusionSpace(),
        ChildAvailableSize().inline_size);
  }

  container_builder_.AddOutOfFlowChildCandidate(
      child, static_offset, LogicalStaticPosition::kInlineStart,
      LogicalStaticPosition::kBlockStart,
      line_clamp_data_.ShouldHideForPaint());
}

void BlockLayoutAlgorithm::HandleFloat(
    const PreviousInflowPosition& previous_inflow_position,
    BlockNode child,
    const BlockBreakToken* child_break_token) {
  // If we're resuming layout, we must always know our position in the BFC.
  DCHECK(!IsBreakInside(child_break_token) ||
         container_builder_.BfcBlockOffset());
  const auto& constraint_space = GetConstraintSpace();

  // If we don't have a BFC block-offset yet, the "expected" BFC block-offset
  // is used to optimistically place floats.
  BfcOffset origin_bfc_offset = {
      constraint_space.GetBfcOffset().line_offset +
          BorderScrollbarPadding().LineLeft(constraint_space.Direction()),
      container_builder_.BfcBlockOffset()
          ? NextBorderEdge(previous_inflow_position)
          : constraint_space.ExpectedBfcBlockOffset()};

  if (child_break_token) {
    // If there's monolithic content inside the float from a previous page
    // overflowing into this one, move past it. And subtract any such overflow
    // from the parent flow, as floats establish a parallel flow.
    origin_bfc_offset.block_offset += child_break_token->MonolithicOverflow() -
                                      GetBreakToken()->MonolithicOverflow();
  }

  if (GetConstraintSpace().HasBlockFragmentation()) {
    // Forced breaks cannot be specified directly on floats, but if the
    // preceding block has a forced break after, we need to break before this
    // float.
    EBreakBetween break_between =
        container_builder_.JoinedBreakBetweenValue(EBreakBetween::kAuto);
    if (IsForcedBreakValue(constraint_space, break_between)) {
      container_builder_.AddBreakBeforeChild(child, kBreakAppealPerfect,
                                             /* is_forced_break*/ true);
      return;
    }
  }

  UnpositionedFloat unpositioned_float(
      child, child_break_token, ChildAvailableSize(), child_percentage_size_,
      replaced_child_percentage_size_, origin_bfc_offset, constraint_space,
      Style(), FragmentainerCapacityForChildren(),
      FragmentainerOffsetForChildren(), line_clamp_data_.ShouldHideForPaint());

  if (!container_builder_.BfcBlockOffset()) {
    container_builder_.AddAdjoiningObjectTypes(
        unpositioned_float.IsLineLeft(constraint_space.Direction())
            ? kAdjoiningFloatLeft
            : kAdjoiningFloatRight);
    // If we don't have a forced BFC block-offset yet, we'll optimistically
    // place floats at the "expected" BFC block-offset. If this differs from
    // our final BFC block-offset we'll need to re-layout.
    if (!constraint_space.ForcedBfcBlockOffset()) {
      abort_when_bfc_block_offset_updated_ = true;
    }
  }

  PositionedFloat positioned_float =
      PositionFloat(&unpositioned_float, &GetExclusionSpace());

  if (positioned_float.minimum_space_shortage > LayoutUnit()) {
    container_builder_.PropagateSpaceShortage(
        positioned_float.minimum_space_shortage);
  }

  if (positioned_float.break_before_token) {
    DCHECK(constraint_space.HasBlockFragmentation());
    container_builder_.AddBreakToken(positioned_float.break_before_token,
                                     /* is_in_parallel_flow */ true);
    // After breaking before the float, carry on with layout of this
    // container. The float constitutes a parallel flow, and there may be
    // siblings that could still fit in the current fragmentainer.
    return;
  }

  DCHECK_EQ(positioned_float.layout_result->Status(), LayoutResult::kSuccess);

  // TODO(mstensho): There should be a class A breakpoint between a float and
  // another float, and also between a float and an in-flow block.

  const PhysicalFragment& physical_fragment =
      positioned_float.layout_result->GetPhysicalFragment();
  LayoutUnit float_inline_size =
      LogicalFragment(constraint_space.GetWritingDirection(), physical_fragment)
          .InlineSize();

  BfcOffset bfc_offset = {constraint_space.GetBfcOffset().line_offset,
                          container_builder_.BfcBlockOffset().value_or(
                              constraint_space.ExpectedBfcBlockOffset())};

  LogicalOffset logical_offset = LogicalFromBfcOffsets(
      positioned_float.bfc_offset, bfc_offset, float_inline_size,
      container_builder_.InlineSize(), constraint_space.Direction());

  container_builder_.AddResult(*positioned_float.layout_result, logical_offset);
}

LayoutResult::EStatus BlockLayoutAlgorithm::HandleNewFormattingContext(
    LayoutInputNode child,
    const BlockBreakToken* child_break_token,
    PreviousInflowPosition* previous_inflow_position) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK(!child.IsOutOfFlowPositioned());
  DCHECK(child.CreatesNewFormattingContext());
  DCHECK(child.IsBlock());

  const auto& constraint_space = GetConstraintSpace();
  const ComputedStyle& child_style = child.Style();
  const TextDirection direction = constraint_space.Direction();
  InflowChildData child_data =
      ComputeChildData(*previous_inflow_position, child, child_break_token,
                       /* is_new_fc */ true);

  LayoutUnit child_origin_line_offset =
      constraint_space.GetBfcOffset().line_offset +
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
  MarginStrut adjoining_margin_strut(previous_inflow_position->margin_strut);
  adjoining_margin_strut.Append(child_data.margins.block_start,
                                child_style.HasMarginBlockStartQuirk());
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
        container_builder_.GetAdjoiningObjectTypes() & kAdjoiningFloatBoth;

    // If this node, or an arbitrary ancestor had clearance past adjoining
    // floats, we consider the margin "separated". We should *never* attempt to
    // re-resolve the BFC block-offset in this case.
    bool has_clearance_past_adjoining_floats =
        constraint_space.AncestorHasClearancePastAdjoiningFloats() ||
        HasClearancePastAdjoiningFloats(
            container_builder_.GetAdjoiningObjectTypes(), child_style, Style());

    if (has_clearance_past_adjoining_floats) {
      child_bfc_offset_estimate = NextBorderEdge(*previous_inflow_position);
      child_margin_got_separated = true;
    } else if (constraint_space.ForcedBfcBlockOffset()) {
      // This is not the first time we're here. We already have a suggested BFC
      // block offset.
      bfc_offset_already_resolved = true;
      child_bfc_offset_estimate = *constraint_space.ForcedBfcBlockOffset();
      // We require that the BFC block offset be the one we'd get with margins
      // adjoining, margins separated, or if clearance was applied to either of
      // these. Anything else is a bug.
      DCHECK(child_bfc_offset_estimate == adjoining_bfc_offset_estimate ||
             child_bfc_offset_estimate == non_adjoining_bfc_offset_estimate ||
             child_bfc_offset_estimate == constraint_space.ClearanceOffset());
      // Figure out if the child margin has already got separated from the
      // margin strut or not.
      //
      // TODO(mstensho): We get false positives here, if the container was
      // cleared by floats (but the child wasn't). See
      // wpt/css/css-break/class-c-breakpoint-after-float-004.html
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
          &child_style.MarginBlockStartUsing(Style()));
    }

    if (!ResolveBfcBlockOffset(previous_inflow_position,
                               child_bfc_offset_estimate)) {
      // If we need to abort here, it means that we had preceding unpositioned
      // floats. This is only expected if we're here for the first time.
      DCHECK(!bfc_offset_already_resolved);
      return LayoutResult::kBfcBlockOffsetResolved;
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
  BfcOffset child_bfc_offset;
  BoxStrut resolved_margins;
  const LayoutResult* layout_result = LayoutNewFormattingContext(
      child, child_break_token, child_data,
      {child_origin_line_offset, child_bfc_offset_estimate}, abort_if_cleared,
      &child_bfc_offset, &resolved_margins);

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
      DCHECK(!constraint_space.AncestorHasClearancePastAdjoiningFloats());
      ResolveBfcBlockOffset(previous_inflow_position,
                            non_adjoining_bfc_offset_estimate,
                            /* forced_bfc_block_offset */ std::nullopt);

      if ((bfc_offset_already_resolved || has_adjoining_floats) &&
          old_offset != *container_builder_.BfcBlockOffset()) {
        // The first BFC block offset resolution turned out to be wrong, and we
        // positioned preceding adjacent floats based on that. Now we have to
        // roll back and position them at the correct offset. The only expected
        // incorrect estimate is with the child's margin adjoining. Any other
        // incorrect estimate will result in failed layout.
        DCHECK_EQ(old_offset, adjoining_bfc_offset_estimate);
        return LayoutResult::kBfcBlockOffsetResolved;
      }
    }

    child_bfc_offset_estimate = non_adjoining_bfc_offset_estimate;
    child_margin_got_separated = true;

    // We can re-layout the child right away. This re-layout *must* produce a
    // fragment which fits within the exclusion space.
    layout_result = LayoutNewFormattingContext(
        child, child_break_token, child_data,
        {child_origin_line_offset, child_bfc_offset_estimate},
        /* abort_if_cleared */ false, &child_bfc_offset, &resolved_margins);
  }

  if (constraint_space.HasBlockFragmentation()) {
    bool has_container_separation =
        has_break_opportunity_before_next_child_ ||
        child_bfc_offset.block_offset > child_bfc_offset_estimate ||
        layout_result->IsPushedByFloats();
    BreakStatus break_status = BreakBeforeChildIfNeeded(
        child, *layout_result, previous_inflow_position,
        child_bfc_offset.block_offset, has_container_separation);
    if (break_status == BreakStatus::kBrokeBefore) {
      return LayoutResult::kSuccess;
    }
    if (break_status == BreakStatus::kNeedsEarlierBreak) {
      return LayoutResult::kNeedsEarlierBreak;
    }

    // If the child aborted layout, we cannot continue.
    DCHECK_EQ(layout_result->Status(), LayoutResult::kSuccess);
  }

  const auto& physical_fragment = layout_result->GetPhysicalFragment();
  LogicalFragment fragment(constraint_space.GetWritingDirection(),
                           physical_fragment);

  LogicalOffset logical_offset = LogicalFromBfcOffsets(
      child_bfc_offset, ContainerBfcOffset(), fragment.InlineSize(),
      container_builder_.InlineSize(), constraint_space.Direction());

  if (!PositionOrPropagateListMarker(*layout_result, &logical_offset,
                                     previous_inflow_position))
    return LayoutResult::kBfcBlockOffsetResolved;

  PropagateBaselineFromBlockChild(physical_fragment, resolved_margins,
                                  logical_offset.block_offset);

  container_builder_.AddResult(*layout_result, logical_offset,
                               resolved_margins);

  if (!child_break_token || !child_break_token->IsInParallelFlow()) {
    *previous_inflow_position = ComputeInflowPosition(
        *previous_inflow_position, child, child_data,
        child_bfc_offset.block_offset, logical_offset, *layout_result, fragment,
        /* self_collapsing_child_had_clearance */ false);
  }

  // Update line-clamp data, and abort if needed
  if (!line_clamp_data_.UpdateAfterLayout(
          layout_result, *container_builder_.BfcBlockOffset(),
          *previous_inflow_position, Padding().block_end)) {
    container_builder_.SetLinesUntilClamp(
        line_clamp_data_.LinesUntilClamp(/*show_measured_lines*/ true));
    return LayoutResult::kNeedsLineClampRelayout;
  }

  if (constraint_space.HasBlockFragmentation() &&
      !has_break_opportunity_before_next_child_) {
    has_break_opportunity_before_next_child_ =
        HasBreakOpportunityBeforeNextChild(physical_fragment,
                                           child_break_token);
  }

  return LayoutResult::kSuccess;
}

const LayoutResult* BlockLayoutAlgorithm::LayoutNewFormattingContext(
    LayoutInputNode child,
    const BlockBreakToken* child_break_token,
    const InflowChildData& child_data,
    BfcOffset origin_offset,
    bool abort_if_cleared,
    BfcOffset* out_child_bfc_offset,
    BoxStrut* out_resolved_margins) {
  const auto& style = Style();
  const auto& child_style = child.Style();
  const TextDirection direction = GetConstraintSpace().Direction();
  const auto writing_direction = GetConstraintSpace().GetWritingDirection();

  if (!IsBreakInside(child_break_token)) {
    // The origin offset is where we should start looking for layout
    // opportunities. It needs to be adjusted by the child's clearance.
    AdjustToClearance(GetExclusionSpace().ClearanceOffsetIncludingInitialLetter(
                          child_style.Clear(style)),
                      &origin_offset);
  }
  DCHECK(container_builder_.BfcBlockOffset());

  LayoutOpportunityVector opportunities =
      GetExclusionSpace().AllLayoutOpportunities(
          origin_offset, ChildAvailableSize().inline_size);
  ClearCollectionScope scope(&opportunities);

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

    // Determine which sides of the opportunity have floats we should avoid.
    // We can detect this when the opportunity-rect sides match the
    // available-rect sides.
    bool has_floats_on_line_left =
        opportunity.rect.LineStartOffset() != origin_offset.line_offset;
    bool has_floats_on_line_right =
        opportunity.rect.LineEndOffset() !=
        (origin_offset.line_offset + ChildAvailableSize().inline_size);
    bool can_expand_outside_opportunity =
        !has_floats_on_line_left && !has_floats_on_line_right;

    const LayoutUnit line_left_margin = child_data.margins.LineLeft(direction);
    const LayoutUnit line_right_margin =
        child_data.margins.LineRight(direction);

    // Find the available inline-size which should be given to the child.
    LayoutUnit line_left_offset = opportunity.rect.LineStartOffset();
    LayoutUnit line_right_offset = opportunity.rect.LineEndOffset();

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

    ConstraintSpace child_space = CreateConstraintSpaceForChild(
        child, child_break_token, child_data,
        {child_available_inline_size, ChildAvailableSize().block_size},
        /* is_new_fc */ true, opportunity.rect.start_offset.block_offset);

    // All formatting context roots (like this child) should start with an empty
    // exclusion space.
    DCHECK(child_space.GetExclusionSpace().IsEmpty());

    const LayoutResult* layout_result = LayoutBlockChild(
        child_space, child_break_token, early_break_,
        /* column_spanner_path */ nullptr, &To<BlockNode>(child));

    // Since this child establishes a new formatting context, no exclusion space
    // should be returned.
    DCHECK(layout_result->GetExclusionSpace().IsEmpty());

    DCHECK_EQ(layout_result->Status(), LayoutResult::kSuccess);

    // Check if we can fit in the opportunity block direction.
    LogicalFragment fragment(writing_direction,
                             layout_result->GetPhysicalFragment());
    if (fragment.BlockSize() > opportunity.rect.BlockSize())
      continue;

    // Now find the fragment's (final) position calculating the auto margins.
    BoxStrut auto_margins = child_data.margins;
    LayoutUnit text_align_offset;
    bool has_auto_margins = false;
    if (child.IsListMarker()) {
      // Deal with marker's margin. It happens only when marker needs to occupy
      // the whole line.
      DCHECK(child.ListMarkerOccupiesWholeLine());
      // Because the marker is laid out as a normal block child, its inline
      // size is extended to fill up the space. Compute the regular marker size
      // from the first child.
      const auto& marker_fragment = layout_result->GetPhysicalFragment();
      LayoutUnit marker_inline_size;
      if (!marker_fragment.Children().empty()) {
        marker_inline_size =
            LogicalFragment(writing_direction,
                            *marker_fragment.Children().front())
                .InlineSize();
      }
      auto_margins.inline_start = UnpositionedListMarker(To<BlockNode>(child))
                                      .InlineOffset(marker_inline_size);
      auto_margins.inline_end = opportunity.rect.InlineSize() -
                                fragment.InlineSize() -
                                auto_margins.inline_start;
    } else {
      if (child_style.MarginInlineStartUsing(style).IsAuto() ||
          child_style.MarginInlineEndUsing(style).IsAuto()) {
        has_auto_margins = true;
        ResolveInlineAutoMargins(child_style, style,
                                 child_available_inline_size,
                                 fragment.InlineSize(), &auto_margins);
      } else {
        // Handle -webkit- values for text-align.
        text_align_offset = WebkitTextAlignAndJustifySelfOffset(
            child_style, style, opportunity.rect.InlineSize(),
            child_data.margins, [&]() { return fragment.InlineSize(); });
      }
    }

    // Determine our final BFC offset.
    //
    // NOTE: |auto_margins| are initialized as a copy of the child's initial
    // margins. To determine the effect of the auto-margins we apply only the
    // difference.
    BfcOffset child_bfc_offset = {LayoutUnit(),
                                  opportunity.rect.BlockStartOffset()};
    if (direction == TextDirection::kLtr) {
      LayoutUnit auto_margin_line_left =
          auto_margins.LineLeft(direction) - line_left_margin;
      child_bfc_offset.line_offset =
          line_left_offset + auto_margin_line_left + text_align_offset;
    } else {
      LayoutUnit auto_margin_line_right =
          auto_margins.LineRight(direction) - line_right_margin;
      child_bfc_offset.line_offset = line_right_offset - text_align_offset -
                                     auto_margin_line_right -
                                     fragment.InlineSize();
    }

    // Check if we'll intersect any floats on our line-left/line-right.
    if (has_floats_on_line_left &&
        child_bfc_offset.line_offset < opportunity.rect.LineStartOffset())
      continue;
    if (has_floats_on_line_right &&
        child_bfc_offset.line_offset + fragment.InlineSize() >
            opportunity.rect.LineEndOffset())
      continue;

    // If we can't expand outside our opportunity, check if we fit in the
    // inline direction.
    if (!can_expand_outside_opportunity &&
        fragment.InlineSize() > opportunity.rect.InlineSize())
      continue;

    // auto-margins are "fun". To ensure round tripping from getComputedStyle
    // the used values are relative to the content-box edge, rather than the
    // opportunity edge.
    BoxStrut resolved_margins = child_data.margins;
    if (has_auto_margins) {
      LayoutUnit inline_offset =
          LogicalFromBfcLineOffset(child_bfc_offset.line_offset,
                                   container_builder_.BfcLineOffset(),
                                   fragment.InlineSize(),
                                   container_builder_.InlineSize(), direction) -
          BorderScrollbarPadding().inline_start;
      if (child_style.MarginInlineStartUsing(style).IsAuto()) {
        resolved_margins.inline_start = inline_offset;
      }
      if (child_style.MarginInlineEndUsing(style).IsAuto()) {
        resolved_margins.inline_end = ChildAvailableSize().inline_size -
                                      inline_offset - fragment.InlineSize();
      }
    }

    *out_child_bfc_offset = child_bfc_offset;
    *out_resolved_margins = resolved_margins;
    return layout_result;
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

LayoutResult::EStatus BlockLayoutAlgorithm::HandleInflow(
    LayoutInputNode child,
    const BreakToken* child_break_token,
    PreviousInflowPosition* previous_inflow_position,
    InlineChildLayoutContext* inline_child_layout_context,
    const InlineBreakToken** previous_inline_break_token) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK(!child.IsOutOfFlowPositioned());
  DCHECK(!child.CreatesNewFormattingContext());

  auto* child_inline_node = DynamicTo<InlineNode>(child);
  if (child_inline_node) {
    // Add reusable line boxes from |previous_result_| if any.
    if (!abort_when_bfc_block_offset_updated_ && !child_break_token &&
        previous_result_) {
      DCHECK(!*previous_inline_break_token);
      if (TryReuseFragmentsFromCache(*child_inline_node,
                                     previous_inflow_position,
                                     previous_inline_break_token))
        return LayoutResult::kSuccess;
    }
  }

  bool has_clearance_past_adjoining_floats =
      !container_builder_.BfcBlockOffset() && child.IsBlock() &&
      HasClearancePastAdjoiningFloats(
          container_builder_.GetAdjoiningObjectTypes(), child.Style(), Style());

  std::optional<LayoutUnit> forced_bfc_block_offset;
  bool is_pushed_by_floats = false;

  // If we can separate the previous margin strut from what is to follow, do
  // that. Then we're able to resolve *our* BFC block offset and position any
  // pending floats. There are two situations where this is necessary:
  //  1. If the child is to be cleared by adjoining floats.
  //  2. If the child is a non-empty inline.
  //
  // Note this logic is copied to TryReuseFragmentsFromCache(), they need to
  // keep in sync.
  if (has_clearance_past_adjoining_floats) {
    if (!ResolveBfcBlockOffset(previous_inflow_position))
      return LayoutResult::kBfcBlockOffsetResolved;

    // If we had clearance past any adjoining floats, we already know where the
    // child is going to be (the child's margins won't have any effect).
    //
    // Set the forced BFC block-offset to the appropriate clearance offset to
    // force this placement of this child.
    if (has_clearance_past_adjoining_floats) {
      forced_bfc_block_offset =
          GetExclusionSpace().ClearanceOffset(child.Style().Clear(Style()));
      is_pushed_by_floats = true;
    }
  }

  // Perform layout on the child.
  InflowChildData child_data =
      ComputeChildData(*previous_inflow_position, child, child_break_token,
                       /* is_new_fc */ false);
  child_data.is_pushed_by_floats = is_pushed_by_floats;
  ConstraintSpace child_space = CreateConstraintSpaceForChild(
      child, child_break_token, child_data, ChildAvailableSize(),
      /* is_new_fc */ false, forced_bfc_block_offset,
      has_clearance_past_adjoining_floats,
      previous_inflow_position->block_end_annotation_space);
  const LayoutResult* layout_result =
      LayoutInflow(child_space, child_break_token, early_break_,
                   column_spanner_path_, &child, inline_child_layout_context);

  // To save space of the stack when we recurse into |BlockNode::Layout|
  // above, the rest of this function is continued within |FinishInflow|.
  // However it should be read as one function.
  return FinishInflow(child, child_break_token, child_space,
                      has_clearance_past_adjoining_floats,
                      std::move(layout_result), &child_data,
                      previous_inflow_position, inline_child_layout_context,
                      previous_inline_break_token);
}

LayoutResult::EStatus BlockLayoutAlgorithm::FinishInflow(
    LayoutInputNode child,
    const BreakToken* child_break_token,
    const ConstraintSpace& child_space,
    bool has_clearance_past_adjoining_floats,
    const LayoutResult* layout_result,
    InflowChildData* child_data,
    PreviousInflowPosition* previous_inflow_position,
    InlineChildLayoutContext* inline_child_layout_context,
    const InlineBreakToken** previous_inline_break_token) {
  if (layout_result->Status() == LayoutResult::kTextBoxTrimEndDidNotApply ||
      (child_space.ShouldTextBoxTrimEnd() &&
       layout_result->Status() == LayoutResult::kSuccess &&
       !layout_result->GetPhysicalFragment().GetBreakToken() &&
       !layout_result->IsBlockEndTrimmed())) [[unlikely]] {
    // If the child algorithm couldn't apply `text-box-trim: end` to the last
    // fragment, block or line, try to apply to the previous child.
    return LayoutResult::kTextBoxTrimEndDidNotApply;
  }

  // If a kNeedsLineClampRelayout layout result was not handled in
  // HandleNonSuccessfulLayoutResult, it needs to be propagated upwards until
  // the BFC root.
  if (layout_result->Status() == LayoutResult::kNeedsLineClampRelayout) {
    DCHECK_EQ(line_clamp_data_.data.state,
              LineClampData::kMeasureLinesUntilBfcOffset);
    container_builder_.SetLinesUntilClamp(layout_result->LinesUntilClamp());
    return LayoutResult::kNeedsLineClampRelayout;
  }

  std::optional<LayoutUnit> child_bfc_block_offset =
      layout_result->BfcBlockOffset();

  bool is_self_collapsing = layout_result->IsSelfCollapsing();

  // "Normal child" here means non-self-collapsing. Even self-collapsing
  // children may be cleared by floats, if they have a forced BFC block-offset.
  bool normal_child_had_clearance =
      layout_result->IsPushedByFloats() && !is_self_collapsing;

  // A child may have aborted its layout if it resolved its BFC block-offset.
  // If we don't have a BFC block-offset yet, we need to propagate the abort
  // signal up to our parent.
  if (layout_result->Status() == LayoutResult::kBfcBlockOffsetResolved &&
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
      if (GetConstraintSpace().ClearanceOffset() ==
          child_space.ClearanceOffset()) {
        container_builder_.SetIsPushedByFloats();
      } else {
        bfc_block_offset = NextBorderEdge(*previous_inflow_position);
      }
    }

    // A new formatting-context may have previously tried to resolve the BFC
    // block-offset. In this case we'll have a "forced" BFC block-offset
    // present, but we shouldn't apply it (instead preferring the child's new
    // BFC block-offset).
    DCHECK(!GetConstraintSpace().AncestorHasClearancePastAdjoiningFloats());

    if (!ResolveBfcBlockOffset(previous_inflow_position, bfc_block_offset,
                               /* forced_bfc_block_offset */ std::nullopt)) {
      return LayoutResult::kBfcBlockOffsetResolved;
    }
  }

  // We have special behavior for a self-collapsing child which gets pushed
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
      return LayoutResult::kBfcBlockOffsetResolved;
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

    if (container_builder_.BfcBlockOffset() &&
        layout_result->Status() == LayoutResult::kSuccess) {
      // Since we know our own BFC block-offset, though, we can calculate that
      // of the child as well.
      child_bfc_block_offset = PositionSelfCollapsingChildWithParentBfc(
          child, child_space, *child_data, *layout_result);

      // We may need to relayout this child if it had any (adjoining) objects
      // which were positioned in the incorrect place.
      if (layout_result->GetPhysicalFragment()
              .HasAdjoiningObjectDescendants() &&
          *child_bfc_block_offset != child_space.ExpectedBfcBlockOffset()) {
        self_collapsing_child_needs_relayout = true;
      }
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
      return LayoutResult::kBfcBlockOffsetResolved;
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
    MarginStrut margin_strut;
    margin_strut.Append(child_data->margins.block_start,
                        child.Style().HasMarginBlockStartQuirk());

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
  if ((layout_result->Status() == LayoutResult::kBfcBlockOffsetResolved ||
       self_collapsing_child_needs_relayout) &&
      child_bfc_block_offset) {
    // Assert that any clearance previously detected isn't lost.
    DCHECK(!child_data->is_pushed_by_floats ||
           layout_result->IsPushedByFloats());
    // If the child got pushed down by floats (normally because of clearance),
    // we need to carry over this state to the next layout pass, as clearance
    // won't automatically be detected then, since the BFC block-offset will
    // already be past the relevant floats.
    child_data->is_pushed_by_floats = layout_result->IsPushedByFloats();

    ConstraintSpace new_child_space = CreateConstraintSpaceForChild(
        child, child_break_token, *child_data, ChildAvailableSize(),
        /* is_new_fc */ false, child_bfc_block_offset);
    layout_result =
        LayoutInflow(new_child_space, child_break_token, early_break_,
                     column_spanner_path_, &child, inline_child_layout_context);

    if (layout_result->Status() == LayoutResult::kBfcBlockOffsetResolved) {
      // Even a second layout pass may abort, if the BFC block offset initially
      // calculated turned out to be wrong. This happens when we discover that
      // an in-flow block-level descendant that establishes a new formatting
      // context doesn't fit beside the floats at its initial position. Allow
      // one more pass.
      child_bfc_block_offset = layout_result->BfcBlockOffset();
      DCHECK(child_bfc_block_offset);

      // We don't expect clearance to be detected at this point. Any clearance
      // should already have been detected above.
      DCHECK(child_data->is_pushed_by_floats ||
             !layout_result->IsPushedByFloats());

      new_child_space = CreateConstraintSpaceForChild(
          child, child_break_token, *child_data, ChildAvailableSize(),
          /* is_new_fc */ false, child_bfc_block_offset);
      layout_result = LayoutInflow(new_child_space, child_break_token,
                                   early_break_, column_spanner_path_, &child,
                                   inline_child_layout_context);
    }

    DCHECK_EQ(layout_result->Status(), LayoutResult::kSuccess);

    // We stored this in a local variable, so it better not have changed.
    DCHECK_EQ(layout_result->IsSelfCollapsing(), is_self_collapsing);
  }

  const std::optional<LayoutUnit> line_box_bfc_block_offset =
      layout_result->LineBoxBfcBlockOffset();

  if (GetConstraintSpace().HasBlockFragmentation()) {
    // If the BFC block-offset is known both for this container and for the
    // child, breaking before may be possible, unless this is a resumed inline
    // formatting context in a parallel block flow. There are situations where
    // such parallel flows cannot be resumed, due to a float (that got pushed
    // from a previous fragmentainer) taking up all the available space in the
    // current fragmentainer, for instance. In such cases we'll just repropagate
    // the break tokens, by obtaining them from inline_child_layout_context
    // below.
    bool consider_breaking_before =
        container_builder_.BfcBlockOffset() && child_bfc_block_offset &&
        (!child.IsInline() || !child_break_token ||
         !To<InlineBreakToken>(child_break_token)->IsInParallelBlockFlow());

    if (consider_breaking_before) {
      bool is_line_box_pushed_by_floats =
          line_box_bfc_block_offset &&
          *line_box_bfc_block_offset > *child_bfc_block_offset;

      // Floats only cause container separation for the outermost block child
      // that gets pushed down (the container and the child may have adjoining
      // block-start margins).
      bool has_container_separation =
          has_break_opportunity_before_next_child_ ||
          (!container_builder_.IsPushedByFloats() &&
           (layout_result->IsPushedByFloats() || is_line_box_pushed_by_floats));

      // If this is a line with a block-in-inline, use the result for the
      // block-in-inline instead of that for the line. That's where we find the
      // relevant info for block fragmentation considerations, including the
      // block break token, if any.
      const LayoutResult& layout_result_to_use =
          container_builder_.LayoutResultForPropagation(*layout_result);

      BreakStatus break_status = BreakBeforeChildIfNeeded(
          child, layout_result_to_use, previous_inflow_position,
          line_box_bfc_block_offset.value_or(*child_bfc_block_offset),
          has_container_separation);
      if (break_status == BreakStatus::kBrokeBefore) {
        return LayoutResult::kSuccess;
      }
      if (break_status == BreakStatus::kNeedsEarlierBreak) {
        return LayoutResult::kNeedsEarlierBreak;
      }
    }

    if (inline_child_layout_context) {
      for (auto token :
           inline_child_layout_context->ParallelFlowBreakTokens()) {
        container_builder_.AddBreakToken(std::move(token),
                                         /* is_in_parallel_flow */ true);
      }
    }
  }

  // It is now safe to update our version of the exclusion space, and any
  // propagated adjoining floats.
  container_builder_.SetExclusionSpace(layout_result->GetExclusionSpace());

  // Only self-collapsing children should have adjoining objects.
  DCHECK(!layout_result->GetAdjoiningObjectTypes() || is_self_collapsing);
  container_builder_.SetAdjoiningObjectTypes(
      layout_result->GetAdjoiningObjectTypes());

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
        layout_result->GetAdjoiningObjectTypes();
    // If our BFC block offset is unknown, and the child got pushed down by
    // floats, so will we.
    if (layout_result->IsPushedByFloats())
      container_builder_.SetIsPushedByFloats();
  }

  const auto& physical_fragment = layout_result->GetPhysicalFragment();
  LogicalFragment fragment(GetConstraintSpace().GetWritingDirection(),
                           physical_fragment);

  if (line_box_bfc_block_offset)
    child_bfc_block_offset = line_box_bfc_block_offset;

  LogicalOffset logical_offset = CalculateLogicalOffset(
      fragment, layout_result->BfcLineOffset(), child_bfc_block_offset);
  if (child.IsSliderThumb()) [[unlikely]] {
    logical_offset = AdjustSliderThumbInlineOffset(fragment, logical_offset);
  }

  if (!PositionOrPropagateListMarker(*layout_result, &logical_offset,
                                     previous_inflow_position))
    return LayoutResult::kBfcBlockOffsetResolved;

  if (physical_fragment.IsLineBox()) {
    PropagateBaselineFromLineBox(physical_fragment,
                                 logical_offset.block_offset);
  } else {
    PropagateBaselineFromBlockChild(physical_fragment, child_data->margins,
                                    logical_offset.block_offset);
  }

  if (IsA<BlockNode>(child)) {
    container_builder_.AddResult(*layout_result, logical_offset,
                                 child_data->margins);
  } else {
    container_builder_.AddResult(*layout_result, logical_offset);
  }

  if (!child_break_token || !child_break_token->IsInParallelFlow()) {
    *previous_inflow_position = ComputeInflowPosition(
        *previous_inflow_position, child, *child_data, child_bfc_block_offset,
        logical_offset, *layout_result, fragment,
        self_collapsing_child_had_clearance);
  }

  if (child.IsInline()) {
    *previous_inline_break_token =
        To<InlineBreakToken>(physical_fragment.GetBreakToken());
  } else {
    *previous_inline_break_token = nullptr;
  }

  // Update |line_clamp_data_| from the LayoutResult, and abort if needed.
  // If the BFC block offset hasn't been resolved, the child we just laid out
  // must be empty (no lines and zero block size), so we can skip the update.
  if (auto bfc_block_offset = container_builder_.BfcBlockOffset()) {
    if (!line_clamp_data_.UpdateAfterLayout(layout_result, *bfc_block_offset,
                                            *previous_inflow_position,
                                            Padding().block_end)) {
      container_builder_.SetLinesUntilClamp(
          line_clamp_data_.LinesUntilClamp(/*show_measured_lines*/ true));
      return LayoutResult::kNeedsLineClampRelayout;
    }
  }

  if (should_text_box_trim_start_ || should_text_box_trim_end_) [[unlikely]] {
    // Update `should_text_box_trim_{start,end}_` if the child `layout_result`
    // has applied `text-box-trim`.
    if (should_text_box_trim_start_ && layout_result->IsBlockStartTrimmed()) {
      should_text_box_trim_start_ = false;
      container_builder_.SetIsBlockStartTrimmed();
    }
    if (should_text_box_trim_end_ &&
        line_clamp_data_.data.state ==
            LineClampData::kMeasureLinesUntilBfcOffset &&
        layout_result->GetPhysicalFragment().GetBreakToken()) {
      // If we trimmed the end only because we're in the first layout of a
      // line-clamp: auto context, and we might not trim in the relayout, then
      // we don't reset should_text_box_trim_end_, and we add the trim length to
      // the logical block offset so next lines are set in the right position.
      DCHECK(layout_result->TrimBlockEndBy());
      previous_inflow_position->logical_block_offset +=
          *layout_result->TrimBlockEndBy();
    } else if (should_text_box_trim_end_) {
      if (layout_result->IsBlockEndTrimmed()) {
        should_text_box_trim_end_ = false;
        container_builder_.SetIsBlockEndTrimmed();
      } else if (!layout_result->IsSelfCollapsing()) {
        // Keep the last non-empty child for `RelayoutForTextBoxTrimEnd`.
        last_non_empty_inflow_child_ = child;
        last_non_empty_break_token_ = child_break_token;
      }
    }
  }

  if (GetConstraintSpace().HasBlockFragmentation() &&
      !has_break_opportunity_before_next_child_) {
    has_break_opportunity_before_next_child_ =
        HasBreakOpportunityBeforeNextChild(physical_fragment,
                                           child_break_token);
  }

  return LayoutResult::kSuccess;
}

InflowChildData BlockLayoutAlgorithm::ComputeChildData(
    const PreviousInflowPosition& previous_inflow_position,
    LayoutInputNode child,
    const BreakToken* child_break_token,
    bool is_new_fc) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK_EQ(is_new_fc, child.CreatesNewFormattingContext());

  // Calculate margins in parent's writing mode.
  LayoutUnit additional_line_offset;
  BoxStrut margins =
      CalculateMargins(child, is_new_fc, &additional_line_offset);

  // Append the current margin strut with child's block start margin.
  // Non empty border/padding, and new formatting-context use cases are handled
  // inside of the child's layout
  MarginStrut margin_strut = previous_inflow_position.margin_strut;

  LayoutUnit logical_block_offset =
      previous_inflow_position.logical_block_offset;

  const auto* child_block_break_token =
      DynamicTo<BlockBreakToken>(child_break_token);
  if (child_block_break_token) [[unlikely]] {
    AdjustMarginsForFragmentation(child_block_break_token, &margins);
    if (child_block_break_token->IsForcedBreak()) {
      // After a forced fragmentainer break we need to reset the margin strut,
      // in case it was set to discard all margins (which is the default at
      // breaks). Margins after a forced break should be retained.
      margin_strut = MarginStrut();
    }

    if (child_block_break_token->MonolithicOverflow() &&
        (Node().IsPaginatedRoot() || !GetBreakToken()->MonolithicOverflow())) {
      // Every container that needs to be pushed to steer clear of monolithic
      // overflow on a previous page will have this stored in its break token.
      // So we'll only add the additional offset here if the child is the
      // outermost container with monolithic overflow recorded.
      logical_block_offset += child_block_break_token->MonolithicOverflow();
    }
  }

  margin_strut.Append(margins.block_start,
                      child.Style().HasMarginBlockStartQuirk());
  if (child.IsBlock())
    SetSubtreeModifiedMarginStrutIfNeeded(&child.Style().MarginBlockStart());

  TextDirection direction = GetConstraintSpace().Direction();
  BfcOffset child_bfc_offset = {
      GetConstraintSpace().GetBfcOffset().line_offset +
          BorderScrollbarPadding().LineLeft(direction) +
          additional_line_offset + margins.LineLeft(direction),
      BfcBlockOffset() + logical_block_offset};

  return InflowChildData(child_bfc_offset, margin_strut, margins);
}

PreviousInflowPosition BlockLayoutAlgorithm::ComputeInflowPosition(
    const PreviousInflowPosition& previous_inflow_position,
    const LayoutInputNode child,
    const InflowChildData& child_data,
    const std::optional<LayoutUnit>& child_bfc_block_offset,
    const LogicalOffset& logical_offset,
    const LayoutResult& layout_result,
    const LogicalFragment& fragment,
    bool self_collapsing_child_had_clearance) {
  // Determine the child's end logical offset, for the next child to use.
  LayoutUnit logical_block_offset;
  std::optional<LayoutUnit> clearance_after_line;
  std::optional<LayoutUnit> trim_block_end_by;

  const bool is_self_collapsing = layout_result.IsSelfCollapsing();
  if (is_self_collapsing) {
    // The default behavior for self-collapsing children is they just pass
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
    logical_block_offset = logical_offset.block_offset + fragment.BlockSize();

    clearance_after_line = layout_result.ClearanceAfterLine();
    trim_block_end_by = layout_result.TrimBlockEndBy();
    if (trim_block_end_by) {
      // Trim the space to respect the `text-box-trim` property here. Objects
      // that pushes following boxes down (e.g., Ruby annotations) are also
      // trimmed.
      logical_block_offset -= *trim_block_end_by;

      if (clearance_after_line) {
        // `<br>` with clearance is an exception. It still pushes down, after
        // all other objects are trimmed. See `AddAnyClearanceAfterLine()`.
        logical_block_offset += *clearance_after_line;
      }
    } else {
      // We add the greater of AnnotationOverflow and ClearanceAfterLine here.
      // Then, we cancel the AnnotationOverflow part if
      //  - The next line box has block-start annotation space, or
      //  - There are no following child boxes and this container has block-end
      //    padding.
      //
      // See InlineLayoutAlgorithm::CreateLine() and
      // BlockLayoutAlgorithm::Layout().
      logical_block_offset +=
          std::max(layout_result.AnnotationOverflow(),
                   clearance_after_line.value_or(LayoutUnit()));
    }
  }

  MarginStrut margin_strut = layout_result.EndMarginStrut();

  // Self collapsing child's end margin can "inherit" quirkiness from its start
  // margin. E.g.
  // <ol style="margin-bottom: 20px"></ol>
  bool is_quirky =
      (is_self_collapsing && child.Style().HasMarginBlockStartQuirk()) ||
      child.Style().HasMarginBlockEndQuirk();
  margin_strut.Append(child_data.margins.block_end, is_quirky);
  if (child.IsBlock())
    SetSubtreeModifiedMarginStrutIfNeeded(&child.Style().MarginBlockEnd());

  if (GetConstraintSpace().HasBlockFragmentation()) [[unlikely]] {
    // If the child broke inside, don't apply any trailing margin, since it's
    // only to be applied to the last fragment that's not in a parallel flow
    // (due to overflow). While trailing margins are normally truncated at
    // fragmentainer boundaries, so that whether or not we add such margins
    // doesn't really make much of a difference, this isn't the case in the
    // initial column balancing pass.
    if (const auto* physical_fragment = DynamicTo<PhysicalBoxFragment>(
            &layout_result.GetPhysicalFragment())) {
      if (const BlockBreakToken* token = physical_fragment->GetBreakToken()) {
        // TODO(mstensho): Don't apply the margin to all overflowing fragments
        // (if any). It should only be applied after the fragment where we
        // reached the block-end of the node.
        if (!token->IsAtBlockEnd())
          margin_strut = MarginStrut();
      }
    }
  }

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

  LayoutUnit annotation_space;
  if (!is_self_collapsing && !trim_block_end_by) {
    annotation_space = layout_result.BlockEndAnnotationSpace();
    if (layout_result.AnnotationOverflow() > LayoutUnit()) {
      DCHECK(!annotation_space);
      // Allow the portion of the annotation overflow that isn't also part of
      // clearance to overlap with certain types of subsequent content.
      annotation_space = -std::max(
          LayoutUnit(), layout_result.AnnotationOverflow() -
                            clearance_after_line.value_or(LayoutUnit()));
    }
  }

  return {logical_block_offset, margin_strut, annotation_space,
          self_or_sibling_self_collapsing_child_had_clearance};
}

LayoutUnit BlockLayoutAlgorithm::PositionSelfCollapsingChildWithParentBfc(
    const LayoutInputNode& child,
    const ConstraintSpace& child_space,
    const InflowChildData& child_data,
    const LayoutResult& layout_result) const {
  DCHECK(layout_result.IsSelfCollapsing());

  // The child must be an in-flow zero-block-size fragment, use its end margin
  // strut for positioning.
  LayoutUnit child_bfc_block_offset =
      child_data.bfc_offset_estimate.block_offset +
      layout_result.EndMarginStrut().Sum();

  ApplyClearance(child_space, &child_bfc_block_offset);

  return child_bfc_block_offset;
}

void BlockLayoutAlgorithm::ConsumeRemainingFragmentainerSpace(
    PreviousInflowPosition* previous_inflow_position) {
  if (GetConstraintSpace().HasKnownFragmentainerBlockSize()) {
    // The remaining part of the fragmentainer (the unusable space for child
    // content, due to the break) should still be occupied by this
    // container. Also encompass fragmentainer overflow (may be caused by
    // monolithic content).
    previous_inflow_position->logical_block_offset =
        std::max(previous_inflow_position->logical_block_offset,
                 FragmentainerSpaceLeftForChildren());
  }
}

BreakStatus BlockLayoutAlgorithm::FinalizeForFragmentation() {
  if (Node().IsTableCell()) {
    // For table cells, prevent breaking before trailing box decorations, as
    // that might disturb the row stretching machinery, causing an infinite
    // loop. We'd add the stretch amount to the block-size to the content box of
    // the table cell, even though we're past it.
    container_builder_.SetShouldPreventBreakBeforeBlockEndDecorations(true);
  }

  if (Node().IsInlineFormattingContextRoot() && !early_break_ &&
      GetConstraintSpace().HasBlockFragmentation()) {
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
        const auto* breakpoint =
            MakeGarbageCollected<EarlyBreak>(line_number, kBreakAppealPerfect);
        container_builder_.SetEarlyBreak(breakpoint);
        return BreakStatus::kNeedsEarlierBreak;
      }
    } else {
      // Everything could fit in the current fragmentainer, but, depending on
      // what comes after, the best location to break at may be between two of
      // our lines.
      UpdateEarlyBreakBetweenLines();
    }
  }

  if (container_builder_.IsFragmentainerBoxType()) {
    return FinishFragmentationForFragmentainer(&container_builder_);
  }

  return FinishFragmentation(&container_builder_);
}

BreakStatus BlockLayoutAlgorithm::BreakBeforeChildIfNeeded(
    LayoutInputNode child,
    const LayoutResult& layout_result,
    PreviousInflowPosition* previous_inflow_position,
    LayoutUnit bfc_block_offset,
    bool has_container_separation) {
  DCHECK(GetConstraintSpace().HasBlockFragmentation());

  // If the BFC offset is unknown, there's nowhere to break, since there's no
  // non-empty child content yet (as that would have resolved the BFC offset).
  DCHECK(container_builder_.BfcBlockOffset());

  LayoutUnit fragmentainer_block_offset =
      FragmentainerOffsetAtBfc(container_builder_) + bfc_block_offset -
      layout_result.AnnotationBlockOffsetAdjustment();

  if (has_container_separation) {
    EBreakBetween break_between =
        CalculateBreakBetweenValue(child, layout_result, container_builder_);
    if (IsForcedBreakValue(GetConstraintSpace(), break_between)) {
      BreakBeforeChild(GetConstraintSpace(), child, &layout_result,
                       fragmentainer_block_offset,
                       FragmentainerCapacityForChildren(), kBreakAppealPerfect,
                       /* is_forced_break */ true, &container_builder_);
      ConsumeRemainingFragmentainerSpace(previous_inflow_position);
      return BreakStatus::kBrokeBefore;
    }
  }

  BreakAppeal appeal_before =
      CalculateBreakAppealBefore(GetConstraintSpace(), child, layout_result,
                                 container_builder_, has_container_separation);

  // Attempt to move past the break point, and if we can do that, also assess
  // the appeal of breaking there, even if we didn't.
  if (MovePastBreakpoint(child, layout_result, fragmentainer_block_offset,
                         appeal_before)) {
    return BreakStatus::kContinue;
  }

  // Figure out where to insert a soft break. It will either be before this
  // child, or before an earlier sibling, if there's a more appealing breakpoint
  // there.

  // Handle line boxes - propagate space shortage and attempt to honor orphans
  // and widows (or detect violations). Skip this part if we didn't produce a
  // fragment (status != kSuccess). The latter happens with BR clear=all if we
  // need to push it to a later fragmentainer to get past floats. BR clear="all"
  // adds clearance *after* the contents (the line), unlike regular CSS
  // clearance, which adds clearance *before* the contents). To handle this
  // corner-case as simply as possible, we'll break (line-wise AND block-wise)
  // before a BR clear=all element, and add it in the fragmentainer where the
  // relevant floats end. This means that we might get an additional line box
  // (to simply hold the BR clear=all), that should be ignored as far as orphans
  // and widows are concerned. Just give up instead, and break before it.
  //
  // Orphans and widows affect column balancing, and if we get imperfect breaks
  // (such as widows / orphans violations), we'll attempt to stretch the
  // columns, and without this exception for BR clear=all, we'd end up
  // stretching to fit the entire float(s) (that could otherwise be broken
  // nicely into fragments) in a single column.
  if (child.IsInline() && layout_result.Status() == LayoutResult::kSuccess) {
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
      PropagateSpaceShortage(
          GetConstraintSpace(), &layout_result, fragmentainer_block_offset,
          FragmentainerCapacityForChildren(), &container_builder_);
    }
    // Attempt to honor orphans and widows requests.
    if (int line_count = container_builder_.LineCount()) {
      if (!first_overflowing_line_)
        first_overflowing_line_ = line_count;
      bool is_first_fragment = !GetBreakToken();
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
          return BreakStatus::kContinue;
        }

        // We have determined that there are plenty of lines for the next
        // fragment, so we can just break exactly where we ran out of space,
        // rather than pushing some of the line boxes over to the next fragment.
      }
      fit_all_lines_ = true;
    }
  }

  if (!AttemptSoftBreak(GetConstraintSpace(), child, &layout_result,
                        fragmentainer_block_offset,
                        FragmentainerCapacityForChildren(), appeal_before,
                        &container_builder_)) {
    return BreakStatus::kNeedsEarlierBreak;
  }

  ConsumeRemainingFragmentainerSpace(previous_inflow_position);
  return BreakStatus::kBrokeBefore;
}

void BlockLayoutAlgorithm::UpdateEarlyBreakBetweenLines() {
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
  BreakAppeal appeal = kBreakAppealPerfect;
  if (line_number < Style().Orphans() ||
      line_count - line_number < Style().Widows()) {
    // Not enough lines in this container to satisfy the orphans and/or widows
    // requirement. If we break before the last line (i.e. the last possible
    // class B breakpoint), we'll fit as much as possible, and that's the best
    // we can do.
    line_number = line_count - 1;
    appeal = kBreakAppealViolatingOrphansAndWidows;
  }
  if (container_builder_.HasEarlyBreak() &&
      container_builder_.GetEarlyBreak().GetBreakAppeal() > appeal) {
    return;
  }
  const auto* breakpoint =
      MakeGarbageCollected<EarlyBreak>(line_number, appeal);
  container_builder_.SetEarlyBreak(breakpoint);
}

BoxStrut BlockLayoutAlgorithm::CalculateMargins(
    LayoutInputNode child,
    bool is_new_fc,
    LayoutUnit* additional_line_offset) {
  DCHECK(child);
  if (child.IsInline())
    return {};

  const ComputedStyle& child_style = child.Style();
  BoxStrut margins =
      ComputeMarginsFor(child_style, child_percentage_size_.inline_size,
                        GetConstraintSpace().GetWritingDirection());
  if (is_new_fc) {
    return margins;
  }

  std::optional<LayoutUnit> child_inline_size;
  auto ChildInlineSize = [&]() -> LayoutUnit {
    if (!child_inline_size) {
      ConstraintSpaceBuilder builder(GetConstraintSpace(),
                                     child_style.GetWritingDirection(),
                                     /* is_new_fc */ false);
      builder.SetAvailableSize(ChildAvailableSize());
      builder.SetPercentageResolutionSize(child_percentage_size_);

      const bool has_auto_margins =
          child_style.MarginInlineStartUsing(Style()).IsAuto() ||
          child_style.MarginInlineEndUsing(Style()).IsAuto();

      const bool justify_self_affects_sizing =
          RuntimeEnabledFeatures::LayoutJustifySelfForBlocksEnabled() &&
          !has_auto_margins;

      const ItemPosition justify_self =
          child_style
              .ResolvedJustifySelf(
                  {ItemPosition::kNormal, OverflowAlignment::kDefault},
                  &Style())
              .GetPosition();

      if (justify_self_affects_sizing &&
          justify_self == ItemPosition::kStretch) {
        builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchExplicit);
      } else if (justify_self_affects_sizing &&
                 justify_self != ItemPosition::kNormal) {
        builder.SetInlineAutoBehavior(AutoSizeBehavior::kFitContent);
      } else {
        builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
      }
      ConstraintSpace space = builder.ToConstraintSpace();

      const auto block_child = To<BlockNode>(child);
      BoxStrut child_border_padding = ComputeBorders(space, block_child) +
                                      ComputePadding(space, child_style);
      child_inline_size = ComputeInlineSizeForFragment(space, block_child,
                                                       child_border_padding);
    }
    return *child_inline_size;
  };

  const auto& style = Style();
  const bool is_rtl = IsRtl(style.Direction());
  const LayoutUnit available_space = ChildAvailableSize().inline_size;

  LayoutUnit text_align_offset;
  if (child_style.MarginInlineStartUsing(style).IsAuto() ||
      child_style.MarginInlineEndUsing(style).IsAuto()) {
    // Resolve auto-margins.
    ResolveInlineAutoMargins(child_style, style, available_space,
                             ChildInlineSize(), &margins);
  } else {
    // Handle -webkit- values for text-align.
    text_align_offset = WebkitTextAlignAndJustifySelfOffset(
        child_style, style, available_space, margins, ChildInlineSize);
  }

  if (is_rtl) {
    *additional_line_offset = ChildAvailableSize().inline_size -
                              text_align_offset - ChildInlineSize() -
                              margins.InlineSum();
  } else {
    *additional_line_offset = text_align_offset;
  }

  return margins;
}

ConstraintSpace BlockLayoutAlgorithm::CreateConstraintSpaceForChild(
    const LayoutInputNode child,
    const BreakToken* child_break_token,
    const InflowChildData& child_data,
    const LogicalSize child_available_size,
    bool is_new_fc,
    const std::optional<LayoutUnit> child_bfc_block_offset,
    bool has_clearance_past_adjoining_floats,
    LayoutUnit block_start_annotation_space) {
  const ComputedStyle& child_style = child.Style();
  const auto child_writing_direction = child_style.GetWritingDirection();
  const auto& constraint_space = GetConstraintSpace();
  ConstraintSpaceBuilder builder(constraint_space, child_writing_direction,
                                 is_new_fc);

  const bool is_in_parallel_flow =
      IsParallelWritingMode(constraint_space.GetWritingMode(),
                            child_writing_direction.GetWritingMode());
  if (!is_in_parallel_flow) [[unlikely]] {
    SetOrthogonalFallbackInlineSize(Style(), child, &builder);
  }

  const bool has_auto_margins =
      child_style.MarginInlineStartUsing(Style()).IsAuto() ||
      child_style.MarginInlineEndUsing(Style()).IsAuto();

  const bool justify_self_affects_sizing =
      RuntimeEnabledFeatures::LayoutJustifySelfForBlocksEnabled() &&
      !has_auto_margins;

  const ItemPosition justify_self =
      child_style
          .ResolvedJustifySelf(
              {ItemPosition::kNormal, OverflowAlignment::kDefault}, &Style())
          .GetPosition();

  if (justify_self_affects_sizing && justify_self == ItemPosition::kStretch) {
    builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchExplicit);
  } else if (justify_self_affects_sizing &&
             justify_self != ItemPosition::kNormal) {
    builder.SetInlineAutoBehavior(AutoSizeBehavior::kFitContent);
  } else if (is_in_parallel_flow &&
             (child.IsInline() ||
              ShouldBlockContainerChildStretchAutoInlineSize(
                  To<BlockNode>(child)))) {
    builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  }

  if (line_clamp_data_.ShouldHideForPaint()) [[unlikely]] {
    builder.SetIsHiddenForPaint(true);
  }

  builder.SetAvailableSize(child_available_size);
  builder.SetPercentageResolutionSize(child_percentage_size_);
  builder.SetReplacedPercentageResolutionSize(replaced_child_percentage_size_);

  if (constraint_space.IsTableCell()) {
    builder.SetIsTableCellChild(true);

    // Always shrink-to-fit children within a <mtd> element.
    if (Node().GetDOMNode() &&
        IsA<MathMLTableCellElement>(Node().GetDOMNode())) {
      builder.SetInlineAutoBehavior(AutoSizeBehavior::kFitContent);
    }

    // Some scrollable percentage-sized children of table-cells use their
    // min-size (instead of sizing normally).
    //
    // We only apply this rule if the block size of the containing table cell
    // is considered to be "restricted". Otherwise, especially if this is the
    // only child of the cell, and that is the only cell in the row, we'd end
    // up with zero block size.
    if (constraint_space.IsRestrictedBlockSizeTableCell() &&
        child_percentage_size_.block_size == kIndefiniteSize &&
        !child.ShouldBeConsideredAsReplaced() &&
        child_style.LogicalHeight().HasPercent() &&
        (child_style.OverflowBlockDirection() == EOverflow::kAuto ||
         child_style.OverflowBlockDirection() == EOverflow::kScroll)) {
      builder.SetIsRestrictedBlockSizeTableCellChild();
    }
  }

  bool has_bfc_block_offset = container_builder_.BfcBlockOffset().has_value();

  // Propagate the |ConstraintSpace::ForcedBfcBlockOffset| down to our
  // children.
  if (!has_bfc_block_offset && constraint_space.ForcedBfcBlockOffset()) {
    builder.SetForcedBfcBlockOffset(*constraint_space.ForcedBfcBlockOffset());
  }
  if (child_bfc_block_offset && !is_new_fc)
    builder.SetForcedBfcBlockOffset(*child_bfc_block_offset);

  if (has_bfc_block_offset) {
    // Typically we aren't allowed to look at the previous layout result within
    // a layout algorithm. However this is fine (honest), as it is just a hint
    // to the child algorithm for where floats should be placed. If it doesn't
    // have this flag, or gets this estimate wrong, it'll relayout with the
    // appropriate "forced" BFC block-offset.
    if (child.IsBlock()) {
      if (const LayoutResult* cached_result =
              child.GetLayoutBox()->GetCachedLayoutResult(
                  To<BlockBreakToken>(child_break_token))) {
        const auto& prev_space = cached_result->GetConstraintSpaceForCaching();

        // To increase the hit-rate we adjust the previous "optimistic"/"forced"
        // BFC block-offset by how much the child has shifted from the previous
        // layout.
        LayoutUnit bfc_block_delta =
            child_data.bfc_offset_estimate.block_offset -
            prev_space.GetBfcOffset().block_offset;
        if (prev_space.ForcedBfcBlockOffset()) {
          builder.SetOptimisticBfcBlockOffset(
              *prev_space.ForcedBfcBlockOffset() + bfc_block_delta);
        } else if (prev_space.OptimisticBfcBlockOffset()) {
          builder.SetOptimisticBfcBlockOffset(
              *prev_space.OptimisticBfcBlockOffset() + bfc_block_delta);
        }
      }
    }
  } else if (constraint_space.OptimisticBfcBlockOffset()) {
    // Propagate the |ConstraintSpace::OptimisticBfcBlockOffset| down to our
    // children.
    builder.SetOptimisticBfcBlockOffset(
        *constraint_space.OptimisticBfcBlockOffset());
  }

  // Propagate the |ConstraintSpace::AncestorHasClearancePastAdjoiningFloats|
  // flag down to our children.
  if (!has_bfc_block_offset &&
      constraint_space.AncestorHasClearancePastAdjoiningFloats()) {
    builder.SetAncestorHasClearancePastAdjoiningFloats();
  }
  if (has_clearance_past_adjoining_floats)
    builder.SetAncestorHasClearancePastAdjoiningFloats();

  LayoutUnit clearance_offset = LayoutUnit::Min();
  if (!IsBreakInside(DynamicTo<BlockBreakToken>(child_break_token))) {
    if (!constraint_space.IsNewFormattingContext()) {
      clearance_offset = constraint_space.ClearanceOffset();
    }
    if (child.IsBlock()) {
      LayoutUnit child_clearance_offset =
          GetExclusionSpace().ClearanceOffset(child_style.Clear(Style()));
      clearance_offset = std::max(clearance_offset, child_clearance_offset);
    }
  }
  builder.SetClearanceOffset(clearance_offset);
  builder.SetBaselineAlgorithmType(constraint_space.GetBaselineAlgorithmType());

  if (child_data.is_pushed_by_floats) {
    // Clearance has been applied, but it won't be automatically detected when
    // laying out the child, since the BFC block-offset has already been updated
    // to be past the relevant floats. We therefore need a flag.
    builder.SetIsPushedByFloats();
  }

  if (!is_new_fc) {
    builder.SetMarginStrut(child_data.margin_strut);
    builder.SetBfcOffset(child_data.bfc_offset_estimate);
    builder.SetExclusionSpace(GetExclusionSpace());
    if (!has_bfc_block_offset) {
      builder.SetAdjoiningObjectTypes(
          container_builder_.GetAdjoiningObjectTypes());
    }
    builder.SetLineClampData(line_clamp_data_.data);
    builder.SetLineClampEndMarginStrut(line_clamp_data_.end_margin_strut);
    builder.SetLineClampEndPadding(Padding().block_end);
  }
  builder.SetBlockStartAnnotationSpace(block_start_annotation_space);

  // Propagate `text-box-trim` only for in-flow children. Check the
  // `LayoutObject` tree, because `InlineNode` synthesizes these flags.
  if ((should_text_box_trim_start_ || should_text_box_trim_end_) &&
      !child.GetLayoutBox()->IsFloatingOrOutOfFlowPositioned()) [[unlikely]] {
    if (should_text_box_trim_start_) {
      builder.SetShouldTextBoxTrimStart();
    }
    if (should_text_box_trim_end_) {
      if (child.IsInline()) {
        // For an inline child, always set the flag. The `InlineLayoutAlgorithm`
        // can determine if it's the last line or not rather quickly. It can
        // still fail for empty lines, which is handled by
        // `RelayoutForTextBoxTrimEnd()`.
        builder.SetShouldTextBoxTrimEnd();
        if (child == override_text_box_trim_end_child_ &&
            InlineBreakToken::IsStartEqual(
                To<InlineBreakToken>(override_text_box_trim_end_break_token_),
                To<InlineBreakToken>(child_break_token))) {
          builder.SetShouldForceTextBoxTrimEnd();
        }
      } else if (IsLastInflowChild(*child.GetLayoutBox()) ||
                 child == override_text_box_trim_end_child_) {
        // For a block child, set the flag only for the last inflow child,
        // because `IsLastInflowChild` can determine the last inflow child
        // rather quickly. It can still fail for empty children, which is
        // handled by `RelayoutForTextBoxTrimEnd()`.
        builder.SetShouldTextBoxTrimEnd();
      }
    }

    // Propagate `text-box-edge` if this box has non-initial `text-box-trim`.
    const ComputedStyle& style = Node().Style();
    builder.SetEffectiveTextBoxEdge(
        style.TextBoxTrim() != ETextBoxTrim::kNone
            ? style.GetTextBoxEdge()
            : constraint_space.EffectiveTextBoxEdge());
  }

  if (constraint_space.HasBlockFragmentation()) {
    LayoutUnit fragmentainer_offset_delta;
    // We need to keep track of our block-offset within the fragmentation
    // context, to be able to tell where the fragmentation line is (i.e. where
    // to break).
    if (is_new_fc) {
      fragmentainer_offset_delta =
          *child_bfc_block_offset - constraint_space.ExpectedBfcBlockOffset();
    } else {
      fragmentainer_offset_delta = builder.ExpectedBfcBlockOffset() -
                                   constraint_space.ExpectedBfcBlockOffset();
    }
    SetupSpaceBuilderForFragmentation(container_builder_, child,
                                      fragmentainer_offset_delta, &builder);

    if (!is_new_fc && GetConstraintSpace().IsInColumnBfc()) {
      // Need to keep track of whether we're in the same formatting context as a
      // column, in order to determine whether column-span:all applies on a
      // descendant.
      builder.SetIsInColumnBfc();
    }

    // If there's a child break inside (typically in a parallel flow, or we
    // would have finished layout by now), we need to produce more
    // fragmentainers, before we can insert any column spanners, so that
    // everything that is supposed to come before the spanner actually ends up
    // there.
    if (constraint_space.IsPastBreak() ||
        container_builder_.HasInsertedChildBreak()) {
      builder.SetIsPastBreak();
    }
  }

  return builder.ToConstraintSpace();
}

void BlockLayoutAlgorithm::PropagateBaselineFromLineBox(
    const PhysicalFragment& child,
    LayoutUnit block_offset) {
  const auto& line_box = To<PhysicalLineBoxFragment>(child);

  // Skip over a line-box which is empty. These don't have any baselines
  // which should be added.
  if (line_box.IsEmptyLineBox())
    return;

  // Skip over the line-box if we are past our clamp point.
  if (line_clamp_data_.IsPastClampPoint()) {
    return;
  }

  if (line_box.IsBlockInInline()) [[unlikely]] {
    // Block-in-inline may have different first/last baselines.
    DCHECK(container_builder_.ItemsBuilder());
    const auto& items =
        container_builder_.ItemsBuilder()->GetLogicalLineItems(line_box);
    const LayoutResult* result = items.BlockInInlineLayoutResult();
    DCHECK(result);
    PropagateBaselineFromBlockChild(result->GetPhysicalFragment(),
                                    /* margins */ BoxStrut(), block_offset);
    return;
  }

  FontHeight metrics = line_box.BaselineMetrics();
  DCHECK(!metrics.IsEmpty());
  LayoutUnit baseline =
      block_offset +
      (Style().IsFlippedLinesWritingMode() ? metrics.descent : metrics.ascent);

  if (!container_builder_.FirstBaseline())
    container_builder_.SetFirstBaseline(baseline);
  container_builder_.SetLastBaseline(baseline);
}

void BlockLayoutAlgorithm::PropagateBaselineFromBlockChild(
    const PhysicalFragment& child,
    const BoxStrut& margins,
    LayoutUnit block_offset) {
  DCHECK(child.IsBox());
  const auto baseline_algorithm =
      GetConstraintSpace().GetBaselineAlgorithmType();

  // When computing baselines for an inline-block, table's don't contribute any
  // baselines.
  if (child.IsTable() &&
      baseline_algorithm == BaselineAlgorithmType::kInlineBlock) {
    return;
  }

  // Skip over the block if we are past our clamp point.
  if (line_clamp_data_.IsPastClampPoint()) {
    return;
  }

  const auto& physical_fragment = To<PhysicalBoxFragment>(child);
  LogicalBoxFragment fragment(GetConstraintSpace().GetWritingDirection(),
                              physical_fragment);

  if (!container_builder_.FirstBaseline()) {
    if (auto first_baseline = fragment.FirstBaseline())
      container_builder_.SetFirstBaseline(block_offset + *first_baseline);
  }

  // Counter-intuitively, when computing baselines for an inline-block, some
  // fragments use their first-baseline for the container's last-baseline.
  bool use_last_baseline =
      baseline_algorithm == BaselineAlgorithmType::kDefault ||
      physical_fragment.UseLastBaselineForInlineBaseline();

  auto last_baseline =
      use_last_baseline ? fragment.LastBaseline() : fragment.FirstBaseline();

  // When computing baselines for an inline-block, some block-boxes (e.g. with
  // "overflow: hidden") will force the baseline to the block-end margin edge.
  if (baseline_algorithm == BaselineAlgorithmType::kInlineBlock &&
      physical_fragment.ForceInlineBaselineSynthesis() &&
      fragment.IsWritingModeEqual()) {
    last_baseline = fragment.BlockSize() + margins.block_end;
  }

  if (last_baseline)
    container_builder_.SetLastBaseline(block_offset + *last_baseline);
}

bool BlockLayoutAlgorithm::ResolveBfcBlockOffset(
    PreviousInflowPosition* previous_inflow_position,
    LayoutUnit bfc_block_offset,
    std::optional<LayoutUnit> forced_bfc_block_offset) {
  // Clearance may have been resolved (along with BFC block-offset) in a
  // previous layout pass, so check the constraint space for pre-applied
  // clearance. This is important in order to identify possible class C break
  // points.
  if (GetConstraintSpace().IsPushedByFloats()) {
    container_builder_.SetIsPushedByFloats();
  }

  if (container_builder_.BfcBlockOffset())
    return true;

  bfc_block_offset = forced_bfc_block_offset.value_or(bfc_block_offset);

  if (ApplyClearance(GetConstraintSpace(), &bfc_block_offset)) {
    container_builder_.SetIsPushedByFloats();
  }

  container_builder_.SetBfcBlockOffset(bfc_block_offset);

  if (NeedsAbortOnBfcBlockOffsetChange()) {
    // A formatting context root should always be able to resolve its
    // whereabouts before layout, so there should never be any incorrect
    // estimates that we need to go back and fix.
    DCHECK(!GetConstraintSpace().IsNewFormattingContext());

    return false;
  }

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
    previous_inflow_position->margin_strut = MarginStrut();
  else
    DCHECK(previous_inflow_position->margin_strut.IsEmpty());

  return true;
}

bool BlockLayoutAlgorithm::NeedsAbortOnBfcBlockOffsetChange() const {
  DCHECK(container_builder_.BfcBlockOffset());
  if (!abort_when_bfc_block_offset_updated_)
    return false;

  // If our position differs from our (potentially optimistic) estimate, abort.
  return *container_builder_.BfcBlockOffset() !=
         GetConstraintSpace().ExpectedBfcBlockOffset();
}

std::optional<LayoutUnit>
BlockLayoutAlgorithm::CalculateQuirkyBodyMarginBlockSum(
    const MarginStrut& end_margin_strut) {
  if (!Node().IsQuirkyAndFillsViewport())
    return std::nullopt;

  if (!Style().LogicalHeight().IsAuto()) {
    return std::nullopt;
  }

  if (GetConstraintSpace().IsNewFormattingContext()) {
    return std::nullopt;
  }

  DCHECK(Node().IsBody());
  LayoutUnit block_end_margin =
      ComputeMarginsForSelf(GetConstraintSpace(), Style()).block_end;

  // The |end_margin_strut| is the block-start margin if the body doesn't have
  // a resolved BFC block-offset.
  if (!container_builder_.BfcBlockOffset())
    return end_margin_strut.Sum() + block_end_margin;

  MarginStrut body_strut = end_margin_strut;
  body_strut.Append(block_end_margin, Style().HasMarginBlockEndQuirk());
  return *container_builder_.BfcBlockOffset() -
         GetConstraintSpace().GetBfcOffset().block_offset + body_strut.Sum();
}

bool BlockLayoutAlgorithm::PositionOrPropagateListMarker(
    const LayoutResult& layout_result,
    LogicalOffset* content_offset,
    PreviousInflowPosition* previous_inflow_position) {
  // If this is not a list-item, propagate unpositioned list markers to
  // ancestors.
  if (!ShouldPlaceUnpositionedListMarker())
    return true;

  // If this is a list item, add the unpositioned list marker as a child.
  UnpositionedListMarker list_marker =
      container_builder_.GetUnpositionedListMarker();
  if (!list_marker)
    return true;
  container_builder_.ClearUnpositionedListMarker();

  const ConstraintSpace& space = GetConstraintSpace();
  const auto& content = layout_result.GetPhysicalFragment();
  FontBaseline baseline_type = Style().GetFontBaseline();
  if (auto content_baseline =
          list_marker.ContentAlignmentBaseline(space, baseline_type, content)) {
    // TODO: We are reusing the ConstraintSpace for LI here. It works well for
    // now because authors cannot style list-markers currently. If we want to
    // support `::marker` pseudo, we need to create ConstraintSpace for marker
    // separately.
    const LayoutResult* marker_layout_result =
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
                         *content_baseline, &content_offset->block_offset,
                         &container_builder_);
    return true;
  }

  // If the list marker could not be positioned against this child because it
  // does not have the baseline to align to, keep it as unpositioned and try
  // the next child.
  container_builder_.SetUnpositionedListMarker(list_marker);
  return true;
}

bool BlockLayoutAlgorithm::PositionListMarkerWithoutLineBoxes(
    PreviousInflowPosition* previous_inflow_position) {
  DCHECK(ShouldPlaceUnpositionedListMarker());
  DCHECK(container_builder_.GetUnpositionedListMarker());

  auto list_marker = container_builder_.GetUnpositionedListMarker();
  const ConstraintSpace& space = GetConstraintSpace();
  FontBaseline baseline_type = Style().GetFontBaseline();
  // Layout the list marker.
  const LayoutResult* marker_layout_result =
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
  list_marker.AddToBoxWithoutLineBoxes(
      space, baseline_type, *marker_layout_result, &container_builder_,
      &intrinsic_block_size_);
  container_builder_.ClearUnpositionedListMarker();

  return true;
}

bool BlockLayoutAlgorithm::IsRubyText(const LayoutInputNode& child) const {
  return Node().IsRubyColumn() && child.IsRubyText();
}

void BlockLayoutAlgorithm::HandleRubyText(BlockNode ruby_text_child) {
  DCHECK(Node().IsRubyColumn());

  const BlockBreakToken* break_token = nullptr;
  if (const auto* token = GetBreakToken()) {
    for (const auto& child_token : token->ChildBreakTokens()) {
      if (child_token->InputNode() == ruby_text_child) {
        break_token = To<BlockBreakToken>(child_token.Get());
        break;
      }
    }
  }

  const ComputedStyle& rt_style = ruby_text_child.Style();
  ConstraintSpaceBuilder builder(GetConstraintSpace(),
                                 rt_style.GetWritingDirection(), true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), ruby_text_child, &builder);
  builder.SetAvailableSize(ChildAvailableSize());
  if (IsParallelWritingMode(GetConstraintSpace().GetWritingMode(),
                            rt_style.GetWritingMode())) {
    builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  }

  if (line_clamp_data_.ShouldHideForPaint()) [[unlikely]] {
    builder.SetIsHiddenForPaint(true);
  }

  const LayoutResult* result =
      ruby_text_child.Layout(builder.ToConstraintSpace(), break_token);

  const auto& ruby_text_fragment =
      To<PhysicalBoxFragment>(result->GetPhysicalFragment());
  const LogicalRect ruby_text_box = ruby_text_fragment.ConvertChildToLogical(
      ComputeRubyEmHeightBox(ruby_text_fragment));

  // Find the ruby-base fragment.
  const PhysicalBoxFragment* ruby_base_fragment = nullptr;
  LayoutUnit ruby_base_block_offset;
  for (const auto& child : container_builder_.Children()) {
    if (child->IsRubyBase()) {
      ruby_base_fragment = &To<PhysicalBoxFragment>(*child.fragment);
      ruby_base_block_offset = child.offset.block_offset;
      break;
    }
  }

  LayoutUnit ruby_text_box_top;
  const RubyPosition block_start_position = Style().IsFlippedLinesWritingMode()
                                                ? RubyPosition::kUnder
                                                : RubyPosition::kOver;
  if (Style().GetRubyPosition() == block_start_position) {
    LayoutUnit last_line_ruby_text_bottom = ruby_text_box.BlockEndOffset();

    // Get the top of the text in the ruby-base.
    LayoutUnit first_line_top;
    if (ruby_base_fragment) {
      first_line_top = ruby_base_block_offset +
                       ruby_base_fragment
                           ->ConvertChildToLogical(
                               ComputeRubyEmHeightBox(*ruby_base_fragment))
                           .offset.block_offset;
    }
    ruby_text_box_top = first_line_top - last_line_ruby_text_bottom;
    const LayoutUnit ruby_text_top =
        ruby_text_box_top + ruby_text_box.offset.block_offset;
    if (ruby_text_top < LayoutUnit())
      container_builder_.SetAnnotationOverflow(ruby_text_top);
  } else {
    LayoutUnit first_line_ruby_text_top = ruby_text_box.offset.block_offset;

    // Get the bottom of the text in the ruby-base.
    LayoutUnit last_line_bottom;
    LayoutUnit base_logical_bottom;
    if (ruby_base_fragment) {
      LayoutUnit base_block_size =
          ruby_base_fragment->Size()
              .ConvertToLogical(Style().GetWritingMode())
              .block_size;
      last_line_bottom = ruby_base_block_offset +
                         ruby_base_fragment
                             ->ConvertChildToLogical(
                                 ComputeRubyEmHeightBox(*ruby_base_fragment))
                             .BlockEndOffset();
      base_logical_bottom = ruby_base_block_offset + base_block_size;
    }
    ruby_text_box_top = last_line_bottom - first_line_ruby_text_top;
    const LayoutUnit logical_bottom_overflow = ruby_text_box_top +
                                               ruby_text_box.BlockEndOffset() -
                                               base_logical_bottom;
    if (logical_bottom_overflow > LayoutUnit())
      container_builder_.SetAnnotationOverflow(logical_bottom_overflow);
  }
  container_builder_.AddResult(*result,
                               LogicalOffset(LayoutUnit(), ruby_text_box_top));
}

LayoutUnit BlockLayoutAlgorithm::HandleTextControlPlaceholder(
    BlockNode placeholder,
    const PreviousInflowPosition& previous_inflow_position) {
  DCHECK(Node().IsTextControl()) << Node().GetLayoutBox();

  const wtf_size_t kTextBlockIndex = 0u;
  LogicalSize available_size = ChildAvailableSize();
  bool apply_fixed_size = Style().ApplyControlFixedSize(Node().GetDOMNode());
  if (container_builder_.Children().size() > 0 && apply_fixed_size) {
    // The placeholder should have the width same as "editing-view-port"
    // element, which is the first grandchild of the text control.
    const PhysicalFragment& child =
        *container_builder_.Children()[kTextBlockIndex].fragment;
    if (child.IsTextControlContainer()) {
      const auto& grand_children = child.PostLayoutChildren();
      const auto begin = grand_children.begin();
      if (begin != grand_children.end()) {
        LogicalFragment grand_child_fragment(
            GetConstraintSpace().GetWritingDirection(), *begin->fragment);
        available_size.inline_size = grand_child_fragment.InlineSize();
      }
    }
  }

  const bool is_new_fc = placeholder.CreatesNewFormattingContext();
  const InflowChildData child_data =
      ComputeChildData(previous_inflow_position, placeholder,
                       /* child_break_token */ nullptr, is_new_fc);
  const ConstraintSpace space = CreateConstraintSpaceForChild(
      placeholder, /* child_break_token */ nullptr, child_data, available_size,
      is_new_fc);

  const LayoutResult* result = placeholder.Layout(space);
  LogicalOffset offset = BorderScrollbarPadding().StartOffset();
  if (Node().IsTextArea()) {
    return FinishTextControlPlaceholder(result, offset, apply_fixed_size,
                                        previous_inflow_position);
  }
  // Usually another child provides the baseline. However it doesn't if
  // another child is out-of-flow.
  if (!container_builder_.FirstBaseline()) {
    return FinishTextControlPlaceholder(result, offset, apply_fixed_size,
                                        previous_inflow_position);
  }
  LogicalBoxFragment fragment(
      GetConstraintSpace().GetWritingDirection(),
      To<PhysicalBoxFragment>(result->GetPhysicalFragment()));
  // We should apply FirstBaseline() of the placeholder fragment because the
  // placeholder might have the 'overflow' property, and its LastBaseline()
  // might be the block-end margin.
  // |fragment| has no FirstBaseline() if it consists of only white-spaces.
  if (fragment.FirstBaseline().has_value()) {
    LayoutUnit first_baseline = *container_builder_.FirstBaseline();
    const LayoutUnit border_padding_block_start =
        BorderScrollbarPadding().block_start;
    const LayoutUnit placeholder_baseline = *fragment.FirstBaseline();
    offset.block_offset = first_baseline - placeholder_baseline;
    if (!apply_fixed_size && offset.block_offset < border_padding_block_start) {
      // The placeholder is taller. We should shift down the existing child.
      const LayoutUnit new_baseline =
          placeholder_baseline + border_padding_block_start;
      container_builder_.SetFirstBaseline(new_baseline);
      container_builder_.SetLastBaseline(new_baseline);
      const LogicalFragmentLink& first_child =
          container_builder_.Children()[kTextBlockIndex];
      LogicalOffset first_child_offset = first_child.offset;
      first_child_offset.block_offset += new_baseline - first_baseline;
      container_builder_.ReplaceChild(kTextBlockIndex, *first_child.fragment,
                                      first_child_offset);
      offset.block_offset = border_padding_block_start;
    }
  }
  return FinishTextControlPlaceholder(result, offset, apply_fixed_size,
                                      previous_inflow_position);
}

LayoutUnit BlockLayoutAlgorithm::FinishTextControlPlaceholder(
    const LayoutResult* result,
    const LogicalOffset& offset,
    bool apply_fixed_size,
    const PreviousInflowPosition& previous_inflow_position) {
  container_builder_.AddResult(*result, offset);
  LayoutUnit block_offset = previous_inflow_position.logical_block_offset;
  if (apply_fixed_size) {
    return block_offset;
  }
  LogicalBoxFragment fragment(
      GetConstraintSpace().GetWritingDirection(),
      To<PhysicalBoxFragment>(result->GetPhysicalFragment()));
  return std::max(block_offset, offset.block_offset + fragment.BlockSize());
}

LogicalOffset BlockLayoutAlgorithm::AdjustSliderThumbInlineOffset(
    const LogicalFragment& fragment,
    const LogicalOffset& logical_offset) {
  const LayoutUnit available_extent =
      ChildAvailableSize().inline_size - fragment.InlineSize();
  const auto* input =
      To<HTMLInputElement>(Node().GetDOMNode()->OwnerShadowHost());
  LayoutUnit offset(input->RatioValue().ToDouble() * available_extent);
  return {logical_offset.inline_offset + offset, logical_offset.block_offset};
}

}  // namespace blink
