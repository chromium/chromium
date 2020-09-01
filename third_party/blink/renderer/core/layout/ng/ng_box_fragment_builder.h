// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_

#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_mathml_paint_info.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_fragment_data.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class NGPhysicalFragment;

class CORE_EXPORT NGBoxFragmentBuilder final
    : public NGContainerFragmentBuilder {
  DISALLOW_NEW();

 public:
  NGBoxFragmentBuilder(NGLayoutInputNode node,
                       scoped_refptr<const ComputedStyle> style,
                       const NGConstraintSpace* space,
                       WritingDirectionMode writing_direction)
      : NGContainerFragmentBuilder(node,
                                   std::move(style),
                                   space,
                                   writing_direction),
        box_type_(NGPhysicalFragment::NGBoxType::kNormalBox),
        is_inline_formatting_context_(node.IsInline()) {}

  // Build a fragment for LayoutObject without NGLayoutInputNode. LayoutInline
  // has NGInlineItem but does not have corresponding NGLayoutInputNode.
  NGBoxFragmentBuilder(LayoutObject* layout_object,
                       scoped_refptr<const ComputedStyle> style,
                       WritingDirectionMode writing_direction)
      : NGContainerFragmentBuilder(/* node */ nullptr,
                                   std::move(style),
                                   /* space */ nullptr,
                                   writing_direction),
        box_type_(NGPhysicalFragment::NGBoxType::kNormalBox),
        is_inline_formatting_context_(true) {
    layout_object_ = layout_object;
  }

  void SetInitialFragmentGeometry(
      const NGFragmentGeometry& initial_fragment_geometry) {
    initial_fragment_geometry_ = &initial_fragment_geometry;
    size_ = initial_fragment_geometry_->border_box_size;
    is_initial_block_size_indefinite_ = size_.block_size == kIndefiniteSize;

    border_padding_ =
        initial_fragment_geometry.border + initial_fragment_geometry.padding;
    border_scrollbar_padding_ =
        border_padding_ + initial_fragment_geometry.scrollbar;
    if (space_) {
      child_available_size_ = CalculateChildAvailableSize(
          *space_, To<NGBlockNode>(node_), size_, border_scrollbar_padding_);
    }
  }

  void AdjustBorderScrollbarPaddingForFragmentation(
      const NGBlockBreakToken* break_token) {
    if (LIKELY(!break_token))
      return;
    if (break_token->IsBreakBefore())
      return;
    border_scrollbar_padding_.block_start = LayoutUnit();
  }

  void AdjustBorderScrollbarPaddingForTableCell() {
    if (!space_->IsTableCell())
      return;
    border_scrollbar_padding_ +=
        ComputeIntrinsicPadding(*space_, *style_, Scrollbar());
  }

  const NGFragmentGeometry& InitialFragmentGeometry() const {
    DCHECK(initial_fragment_geometry_);
    return *initial_fragment_geometry_;
  }

  // Use the block-size setters/getters further down instead of the inherited
  // ones.
  LayoutUnit BlockSize() const = delete;
  void SetBlockSize(LayoutUnit block_size) = delete;

  // Set the total border-box block-size of all the fragments to be generated
  // from this node (as if we stitched them together). Layout algorithms are
  // expected to pass this value, and at the end of layout (if block
  // fragmentation is needed), the fragmentation machinery will be invoked to
  // adjust the block-size to the correct size, ensuring that we break at the
  // best location.
  void SetFragmentsTotalBlockSize(LayoutUnit block_size) {
#if DCHECK_IS_ON()
    // Note that we just store the block-size in a shared field. We have a flag
    // for debugging, to assert that we know what we're doing when attempting to
    // access the data.
    block_size_is_for_all_fragments_ = true;
#endif
    size_.block_size = block_size;
  }
  LayoutUnit FragmentsTotalBlockSize() const {
#if DCHECK_IS_ON()
    if (has_block_fragmentation_)
      DCHECK(block_size_is_for_all_fragments_);
#endif
    return size_.block_size;
  }

  // Set the final block-size of this fragment.
  void SetFragmentBlockSize(LayoutUnit block_size) {
#if DCHECK_IS_ON()
    // Note that we just store the block-size in a shared field. We have a flag
    // for debugging, to assert that we know what we're doing when attempting to
    // access the data.
    block_size_is_for_all_fragments_ = false;
#endif
    size_.block_size = block_size;
  }

  LayoutUnit FragmentBlockSize() const {
#if DCHECK_IS_ON()
    if (has_block_fragmentation_)
      DCHECK(!block_size_is_for_all_fragments_);
#endif
    return size_.block_size;
  }

  void SetOverflowBlockSize(LayoutUnit overflow_block_size) {
    overflow_block_size_ = overflow_block_size;
  }
  void SetIntrinsicBlockSize(LayoutUnit intrinsic_block_size) {
    intrinsic_block_size_ = intrinsic_block_size;
  }
  LayoutUnit IntrinsicBlockSize() const { return intrinsic_block_size_; }
  const NGBoxStrut& Borders() const {
    DCHECK(initial_fragment_geometry_);
    DCHECK_NE(BoxType(), NGPhysicalFragment::kInlineBox);
    return initial_fragment_geometry_->border;
  }
  const NGBoxStrut& Scrollbar() const {
    DCHECK(initial_fragment_geometry_);
    return initial_fragment_geometry_->scrollbar;
  }
  const NGBoxStrut& Padding() const {
    DCHECK(initial_fragment_geometry_);
    return initial_fragment_geometry_->padding;
  }
  const LogicalSize& InitialBorderBoxSize() const {
    DCHECK(initial_fragment_geometry_);
    return initial_fragment_geometry_->border_box_size;
  }
  const NGBoxStrut& BorderPadding() const {
    DCHECK(initial_fragment_geometry_);
    return border_padding_;
  }
  const NGBoxStrut& BorderScrollbarPadding() const {
    DCHECK(initial_fragment_geometry_);
    return border_scrollbar_padding_;
  }
  // The child available-size is subtly different from the content-box size of
  // an element. For an anonymous-block the child available-size is equal to
  // its non-anonymous parent (similar to percentages).
  const LogicalSize& ChildAvailableSize() const {
    DCHECK(initial_fragment_geometry_);
    DCHECK(space_);
    return child_available_size_;
  }
  const NGBlockNode& Node() {
    DCHECK(node_);
    return To<NGBlockNode>(node_);
  }

  // Add a break token for a child that doesn't yet have any fragments, because
  // its first fragment is to be produced in the next fragmentainer. This will
  // add a break token for the child, but no fragment. Break appeal should
  // always be provided for regular in-flow children. For other types of
  // children it may be omitted, if the break shouldn't affect the appeal of
  // breaking inside this container.
  void AddBreakBeforeChild(NGLayoutInputNode child,
                           base::Optional<NGBreakAppeal> appeal,
                           bool is_forced_break);

  // Add a layout result. This involves appending the fragment and its relative
  // offset to the builder, but also keeping track of out-of-flow positioned
  // descendants, propagating fragmentainer breaks, and more.
  void AddResult(const NGLayoutResult&, const LogicalOffset);

  void AddChild(scoped_refptr<const NGPhysicalTextFragment> child,
                const LogicalOffset& offset) {
    AddChildInternal(child, offset);
  }

  void AddChild(const NGPhysicalContainerFragment&,
                const LogicalOffset&,
                const LayoutInline* inline_container = nullptr);

  // Manually add a break token to the builder. Note that we're assuming that
  // this break token is for content in the same flow as this parent.
  void AddBreakToken(scoped_refptr<const NGBreakToken>,
                     bool is_in_parallel_flow = false);

  void AddOutOfFlowLegacyCandidate(NGBlockNode,
                                   const NGLogicalStaticPosition&,
                                   const LayoutInline* inline_container);

  // Specify whether this will be the first fragment generated for the node.
  void SetIsFirstForNode(bool is_first) { is_first_for_node_ = is_first; }

  // Set how much of the block-size we've used so far for this box. This will be
  // the sum of the block-size of all previous fragments PLUS the one we're
  // building now.
  void SetConsumedBlockSize(LayoutUnit size) { consumed_block_size_ = size; }

  // Set how much of the column block-size we've used so far. This will be used
  // to determine the block-size of any new columns added by descendant
  // out-of-flow positioned elements.
  void SetBlockOffsetForAdditionalColumns(LayoutUnit size) {
    block_offset_for_additional_columns_ = size;
  }
  LayoutUnit BlockOffsetForAdditionalColumns() const {
    return block_offset_for_additional_columns_;
  }

  void SetSequenceNumber(unsigned sequence_number) {
    sequence_number_ = sequence_number;
  }

  // Return true if we broke inside this node on our own initiative (typically
  // not because of a child break, but rather due to the size of this node).
  bool DidBreakSelf() const { return did_break_self_; }
  void SetDidBreakSelf() { did_break_self_ = true; }

  // Return true if we need to break before or inside any child, doesn't matter
  // if it's in-flow or not. As long as there are only breaks in parallel flows,
  // we may continue layout, but when we're done, we'll need to create a break
  // token for this fragment nevertheless, so that we re-enter, descend and
  // resume at the broken children in the next fragmentainer.
  bool HasChildBreakInside() const {
    if (!child_break_tokens_.IsEmpty())
      return true;
    // Inline nodes produce a "finished" trailing break token even if we don't
    // need to block-fragment.
    return !inline_break_tokens_.IsEmpty() &&
           !inline_break_tokens_.back()->IsFinished();
  }

  // Return true if we need to break before or inside any in-flow child that
  // doesn't establish a parallel flow. When this happens, we want to finish our
  // fragment, create a break token, and resume in the next fragmentainer.
  bool HasInflowChildBreakInside() const {
    return has_inflow_child_break_inside_;
  }

  // Report space shortage, i.e. how much more space would have been sufficient
  // to prevent some piece of content from breaking. This information may be
  // used by the column balancer to stretch columns.
  void PropagateSpaceShortage(LayoutUnit space_shortage) {
    DCHECK_GT(space_shortage, LayoutUnit());

    // Space shortage should only be reported when we already have a tentative
    // fragmentainer block-size. It's meaningless to talk about space shortage
    // in the initial column balancing pass, because then we have no
    // fragmentainer block-size at all, so who's to tell what's too short or
    // not?
    DCHECK(!IsInitialColumnBalancingPass());

    if (minimal_space_shortage_ > space_shortage)
      minimal_space_shortage_ = space_shortage;
  }
  LayoutUnit MinimalSpaceShortage() const { return minimal_space_shortage_; }

  void PropagateTallestUnbreakableBlockSize(LayoutUnit unbreakable_block_size) {
    // We should only calculate the block-size of the tallest piece of
    // unbreakable content during the initial column balancing pass, when we
    // haven't set a tentative fragmentainer block-size yet.
    DCHECK(IsInitialColumnBalancingPass());

    tallest_unbreakable_block_size_ =
        std::max(tallest_unbreakable_block_size_, unbreakable_block_size);
  }

  void SetIsInitialColumnBalancingPass() {
    // Note that we have no dedicated flag for being in the initial column
    // balancing pass here. We'll just bump tallest_unbreakable_block_size_ to
    // 0, so that NGLayoutResult knows that we need to store unbreakable
    // block-size.
    DCHECK_EQ(tallest_unbreakable_block_size_, LayoutUnit::Min());
    tallest_unbreakable_block_size_ = LayoutUnit();
  }
  bool IsInitialColumnBalancingPass() const {
    return tallest_unbreakable_block_size_ >= LayoutUnit();
  }

  void SetInitialBreakBefore(EBreakBetween break_before) {
    initial_break_before_ = break_before;
  }

  void SetPreviousBreakAfter(EBreakBetween break_after) {
    previous_break_after_ = break_after;
  }

  // Set when this subtree has modified the incoming margin-strut, such that it
  // may change our final position.
  void SetSubtreeModifiedMarginStrut() {
    DCHECK(!BfcBlockOffset());
    subtree_modified_margin_strut_ = true;
  }

  // Join/"collapse" the previous (stored) break-after value with the next
  // break-before value, to determine how to deal with breaking between two
  // in-flow siblings.
  EBreakBetween JoinedBreakBetweenValue(EBreakBetween break_before) const;

  // Return the number of line boxes laid out.
  int LineCount() const { return inline_break_tokens_.size(); }

  // Set when we have iterated over all the children. This means that all
  // children have been fully laid out, or have break tokens. No more children
  // left to discover.
  void SetHasSeenAllChildren() { has_seen_all_children_ = true; }
  bool HasSeenAllChildren() { return has_seen_all_children_; }

  void SetIsAtBlockEnd() { is_at_block_end_ = true; }

  void SetColumnSpanner(NGBlockNode spanner) { column_spanner_ = spanner; }
  bool FoundColumnSpanner() const { return !!column_spanner_; }

  void SetLinesUntilClamp(const base::Optional<int>& value) {
    lines_until_clamp_ = value;
  }

  void SetEarlyBreak(scoped_refptr<const NGEarlyBreak> breakpoint,
                     NGBreakAppeal appeal) {
    early_break_ = breakpoint;
    break_appeal_ = appeal;
  }
  bool HasEarlyBreak() const { return early_break_.get(); }
  const NGEarlyBreak& EarlyBreak() const {
    DCHECK(early_break_.get());
    return *early_break_.get();
  }

  // Set the highest break appeal found so far. This is either:
  // 1: The highest appeal of a breakpoint found by our container
  // 2: The appeal of a possible early break inside
  // 3: The appeal of an actual break inside (to be stored in a break token)
  void SetBreakAppeal(NGBreakAppeal appeal) { break_appeal_ = appeal; }
  NGBreakAppeal BreakAppeal() const { return break_appeal_; }

  // Offsets are not supposed to be set during fragment construction, so we
  // do not provide a setter here.

  // Creates the fragment. Can only be called once.
  scoped_refptr<const NGLayoutResult> ToBoxFragment() {
    DCHECK_NE(BoxType(), NGPhysicalFragment::kInlineBox);
    return ToBoxFragment(GetWritingMode());
  }
  scoped_refptr<const NGLayoutResult> ToInlineBoxFragment() {
    // The logical coordinate for inline box uses line-relative writing-mode,
    // not
    // flow-relative.
    DCHECK_EQ(BoxType(), NGPhysicalFragment::kInlineBox);
    return ToBoxFragment(ToLineWritingMode(GetWritingMode()));
  }

  scoped_refptr<const NGLayoutResult> Abort(NGLayoutResult::EStatus);

  NGPhysicalFragment::NGBoxType BoxType() const;
  void SetBoxType(NGPhysicalFragment::NGBoxType box_type) {
    box_type_ = box_type;
  }
  bool IsFragmentainerBoxType() const {
    return BoxType() == NGPhysicalFragment::kColumnBox;
  }
  void SetIsFieldsetContainer() { is_fieldset_container_ = true; }
  void SetIsLegacyLayoutRoot() { is_legacy_layout_root_ = true; }

  void SetIsInlineFormattingContext(bool is_inline_formatting_context) {
    is_inline_formatting_context_ = is_inline_formatting_context;
  }

  void SetIsMathMLFraction() { is_math_fraction_ = true; }
  void SetMathMLPaintInfo(
      UChar operator_character,
      scoped_refptr<const ShapeResultView> operator_shape_result_view,
      LayoutUnit operator_inline_size,
      LayoutUnit operator_ascent,
      LayoutUnit operator_descent,
      const LayoutUnit* radical_operator_inline_offset,
      const NGBoxStrut* radical_base_margins) {
    if (!mathml_paint_info_)
      mathml_paint_info_ = std::make_unique<NGMathMLPaintInfo>();

    mathml_paint_info_->operator_character = operator_character;
    mathml_paint_info_->operator_shape_result_view =
        std::move(operator_shape_result_view);

    mathml_paint_info_->operator_inline_size = operator_inline_size;
    mathml_paint_info_->operator_ascent = operator_ascent;
    mathml_paint_info_->operator_descent = operator_descent;
    if (radical_base_margins)
      mathml_paint_info_->radical_base_margins = *radical_base_margins;
    if (radical_operator_inline_offset) {
      mathml_paint_info_->radical_operator_inline_offset =
          *radical_operator_inline_offset;
    }
  }

  void SetSidesToInclude(LogicalBoxSides sides_to_include) {
    sides_to_include_ = sides_to_include;
  }

  // Either this function or SetBoxType must be called before ToBoxFragment().
  void SetIsNewFormattingContext(bool is_new_fc) { is_new_fc_ = is_new_fc; }

  void SetCustomLayoutData(
      scoped_refptr<SerializedScriptValue> custom_layout_data) {
    custom_layout_data_ = std::move(custom_layout_data);
  }

  // Sets the alignment baseline for this fragment.
  void SetBaseline(LayoutUnit baseline) { baseline_ = baseline; }
  base::Optional<LayoutUnit> Baseline() const { return baseline_; }

  // Sets the last baseline for this fragment.
  void SetLastBaseline(LayoutUnit baseline) {
    DCHECK_EQ(space_->BaselineAlgorithmType(),
              NGBaselineAlgorithmType::kInlineBlock);
    last_baseline_ = baseline;
  }
  base::Optional<LayoutUnit> LastBaseline() const { return last_baseline_; }

  // The inline block baseline is at the block end margin edge under some
  // circumstances. This function updates |LastBaseline| in such cases.
  void SetLastBaselineToBlockEndMarginEdgeIfNeeded();

  void SetTableGridRect(const PhysicalRect& table_grid_rect) {
    table_grid_rect_ = table_grid_rect;
  }

  void SetTableColumnGeometry(
      const NGTableFragmentData::ColumnGeometries& table_column_geometries) {
    table_column_geometries_ = table_column_geometries;
  }

  void SetTableCollapsedBorders(const NGTableBorders& table_collapsed_borders) {
    table_collapsed_borders_ = &table_collapsed_borders;
  }

  void SetTableCollapsedBordersGeometry(
      std::unique_ptr<NGTableFragmentData::CollapsedBordersGeometry>
          table_collapsed_borders_geometry) {
    table_collapsed_borders_geometry_ =
        std::move(table_collapsed_borders_geometry);
  }

  void SetTableColumnCount(wtf_size_t table_column_count) {
    table_column_count_ = table_column_count;
  }

  void SetTableCellColumnIndex(wtf_size_t table_cell_column_index) {
    table_cell_column_index_ = table_cell_column_index;
  }

  // The |NGFragmentItemsBuilder| for the inline formatting context of this box.
  NGFragmentItemsBuilder* ItemsBuilder() { return items_builder_; }
  void SetItemsBuilder(NGFragmentItemsBuilder* builder) {
    items_builder_ = builder;
  }

  // Returns offset for given child. DCHECK if child not found.
  // Warning: Do not call unless necessary.
  LogicalOffset GetChildOffset(const LayoutObject* child) const;

  // Inline containing block geometry is defined by two rectangles, generated
  // by fragments of the LayoutInline.
  struct InlineContainingBlockGeometry {
    DISALLOW_NEW();
    // Union of fragments generated on the first line.
    PhysicalRect start_fragment_union_rect;
    // Union of fragments generated on the last line.
    PhysicalRect end_fragment_union_rect;
  };

  using InlineContainingBlockMap =
      HashMap<const LayoutObject*,
              base::Optional<InlineContainingBlockGeometry>>;

  // Computes the geometry required for any inline containing blocks.
  // |inline_containing_block_map| is a map whose keys specify which inline
  // containing block geometry is required.
  void ComputeInlineContainerGeometryFromFragmentTree(
      InlineContainingBlockMap* inline_containing_block_map);
  void ComputeInlineContainerGeometry(
      InlineContainingBlockMap* inline_containing_block_map);

#if DCHECK_IS_ON()
  // If we don't participate in a fragmentation context, this method can check
  // that all block fragmentation related fields have their initial value.
  void CheckNoBlockFragmentation() const;
#endif

 private:
  // Update whether we have fragmented in this flow.
  void PropagateBreak(const NGLayoutResult&);

  void SetHasForcedBreak() {
    has_forced_break_ = true;
    minimal_space_shortage_ = LayoutUnit::Max();
  }

  scoped_refptr<const NGLayoutResult> ToBoxFragment(WritingMode);

  const NGFragmentGeometry* initial_fragment_geometry_ = nullptr;
  NGBoxStrut border_padding_;
  NGBoxStrut border_scrollbar_padding_;
  LogicalSize child_available_size_;
  LayoutUnit overflow_block_size_ = kIndefiniteSize;
  LayoutUnit intrinsic_block_size_;

  NGFragmentItemsBuilder* items_builder_ = nullptr;

  NGBlockNode column_spanner_ = nullptr;

  NGPhysicalFragment::NGBoxType box_type_;
  bool may_have_descendant_above_block_start_ = false;
  bool is_fieldset_container_ = false;
  bool is_initial_block_size_indefinite_ = false;
  bool is_inline_formatting_context_;
  bool is_first_for_node_ = true;
  bool did_break_self_ = false;
  bool has_inflow_child_break_inside_ = false;
  bool has_forced_break_ = false;
  bool is_new_fc_ = false;
  bool subtree_modified_margin_strut_ = false;
  bool has_seen_all_children_ = false;
  bool is_math_fraction_ = false;
  bool is_at_block_end_ = false;
  LayoutUnit consumed_block_size_;
  LayoutUnit block_offset_for_additional_columns_;
  unsigned sequence_number_ = 0;

  LayoutUnit minimal_space_shortage_ = LayoutUnit::Max();
  LayoutUnit tallest_unbreakable_block_size_ = LayoutUnit::Min();

  // The break-before value on the initial child we cannot honor. There's no
  // valid class A break point before a first child, only *between* siblings.
  EBreakBetween initial_break_before_ = EBreakBetween::kAuto;

  // The break-after value of the previous in-flow sibling.
  EBreakBetween previous_break_after_ = EBreakBetween::kAuto;

  base::Optional<LayoutUnit> baseline_;
  base::Optional<LayoutUnit> last_baseline_;

  // Table specific types.
  base::Optional<PhysicalRect> table_grid_rect_;
  base::Optional<NGTableFragmentData::ColumnGeometries>
      table_column_geometries_;
  scoped_refptr<const NGTableBorders> table_collapsed_borders_;
  std::unique_ptr<NGTableFragmentData::CollapsedBordersGeometry>
      table_collapsed_borders_geometry_;
  base::Optional<wtf_size_t> table_column_count_;

  // Table cell specific types.
  base::Optional<wtf_size_t> table_cell_column_index_;

  LogicalBoxSides sides_to_include_;

  scoped_refptr<SerializedScriptValue> custom_layout_data_;
  base::Optional<int> lines_until_clamp_;

  std::unique_ptr<NGMathMLPaintInfo> mathml_paint_info_;

#if DCHECK_IS_ON()
  // Describes what size_.block_size represents; either the size of a single
  // fragment (false), or the size of all fragments for a node (true).
  bool block_size_is_for_all_fragments_ = false;
#endif

  friend class NGBlockBreakToken;
  friend class NGPhysicalBoxFragment;
  friend class NGLayoutResult;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_
