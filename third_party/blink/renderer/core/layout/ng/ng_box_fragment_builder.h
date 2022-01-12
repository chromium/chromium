// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_

#include "base/dcheck_is_on.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_mathml_paint_info.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_overflow_calculator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_fragment_data.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class NGPhysicalFragment;

class CORE_EXPORT NGBoxFragmentBuilder final
    : public NGContainerFragmentBuilder {
  STACK_ALLOCATED();

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
    DCHECK(size_.block_size != kIndefiniteSize);
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
    DCHECK(!block_size_is_for_all_fragments_ || !has_block_fragmentation_ ||
           IsInitialColumnBalancingPass());
    DCHECK(size_.block_size != kIndefiniteSize);
#endif
    return size_.block_size;
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
                           absl::optional<NGBreakAppeal> appeal,
                           bool is_forced_break);

  // Add a layout result and propagate info from it. This involves appending the
  // fragment and its relative offset to the builder, but also keeping track of
  // out-of-flow positioned descendants, propagating fragmentainer breaks, and
  // more. In some cases, such as grid, the relative offset may need to be
  // computed ahead of time. If so, a |relative_offset| will be passed
  // in. Otherwise, the relative offset will be calculated as normal.
  // |inline_container| is passed when adding an OOF that is contained by a
  // non-atomic inline.
  void AddResult(
      const NGLayoutResult&,
      const LogicalOffset,
      absl::optional<LogicalOffset> relative_offset = absl::nullopt,
      const NGInlineContainer<LogicalOffset>* inline_container = nullptr);

  // Add a child fragment and propagate info from it. Called by AddResult().
  // Other callers should call AddResult() instead of this when possible, since
  // there is information in the layout result that might need to be propagated.
  void AddChild(
      const NGPhysicalFragment&,
      const LogicalOffset&,
      const NGMarginStrut* margin_strut = nullptr,
      bool is_self_collapsing = false,
      absl::optional<LogicalOffset> relative_offset = absl::nullopt,
      const NGInlineContainer<LogicalOffset>* inline_container = nullptr,
      absl::optional<LayoutUnit> adjustment_for_oof_propagation = LayoutUnit());

  // Manually add a break token to the builder. Note that we're assuming that
  // this break token is for content in the same flow as this parent.
  void AddBreakToken(const NGBreakToken*, bool is_in_parallel_flow = false);

  void AddOutOfFlowLegacyCandidate(NGBlockNode,
                                   const NGLogicalStaticPosition&,
                                   const LayoutInline* inline_container);

  // Remove the fragment previously generated for an out-of-flow positioned flex
  // item inside an out-of-flow legacy flex container. This is a work-around for
  // OOFs being laid out out-of-document-order, which is an issue with the
  // legacy engine (although it's not known to cause any other actual problems
  // than this). We'll call this method to correct a document-out-of-order
  // issue.
  void RemoveOldLegacyOOFFlexItem(const LayoutObject&);

  // Before layout we'll determine whether we can tell for sure that the node
  // (or what's left of it to lay out, in case we've already broken) will fit in
  // the current fragmentainer. If this is the case, we'll know that any
  // block-end padding and border will come at the end of this fragment, and, if
  // it's the first fragment for the node, this will make us ensure some child
  // content before allowing breaks. See MustStayInCurrentFragmentainer().
  void SetIsKnownToFitInFragmentainer(bool b) {
    is_known_to_fit_in_fragmentainer_ = b;
  }
  bool IsKnownToFitInFragmentainer() const {
    return is_known_to_fit_in_fragmentainer_;
  }

  // True if we need to keep some child content in the current fragmentainer
  // before breaking (even that overflows the fragmentainer). We'll do this by
  // refusing last-resort breaks when there's no container separation, and we'll
  // instead overflow the fragmentainer. See MustStayInCurrentFragmentainer().
  void SetRequiresContentBeforeBreaking(bool b) {
    requires_content_before_breaking_ = b;
  }
  bool RequiresContentBeforeBreaking() const {
    return requires_content_before_breaking_;
  }

  // If a node fits in one fragmentainer due to restricted block-size, it must
  // stay there, even if the first piece of child content should require more
  // space than that (which would normally push the entire node into the next
  // fragmentainer, since there typically is no valid breakpoint before the
  // first child - only *between* siblings). Furthermore, any first piece of
  // child content also needs to stay in the current fragmentainer, even if this
  // causes fragmentainer overflow. This is not mandated by any spec, but it is
  // compatible with Gecko, and is required in order to print Google Docs.
  //
  // See https://github.com/w3c/csswg-drafts/issues/6056#issuecomment-951767882
  bool MustStayInCurrentFragmentainer() const {
    return is_known_to_fit_in_fragmentainer_ && is_first_for_node_;
  }

  // Specify whether this will be the first fragment generated for the node.
  void SetIsFirstForNode(bool is_first) { is_first_for_node_ = is_first; }

  // Set how much of the block-size we've used so far for this box. This will be
  // the sum of the block-size of all previous fragments PLUS the one we're
  // building now.
  void SetConsumedBlockSize(LayoutUnit size) {
    EnsureBreakTokenData()->consumed_block_size = size;
  }

  // Set how much to adjust |consumed_block_size_| for legacy write-back. See
  // NGBlockBreakToken::ConsumedBlockSizeForLegacy() for more details.
  void SetConsumedBlockSizeLegacyAdjustment(LayoutUnit adjustment) {
    EnsureBreakTokenData()->consumed_block_size_legacy_adjustment = adjustment;
  }

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
    EnsureBreakTokenData()->sequence_number = sequence_number;
  }

  // During regular layout a break token is created at the end of layout, if
  // required. When re-using a previous fragment and its children, though, we
  // may want to just re-use the break token as well.
  void PresetNextBreakToken(const NGBreakToken* break_token) {
    // We should either do block fragmentation as part of normal layout, or
    // pre-set a break token.
    DCHECK(!did_break_self_);
    DCHECK(child_break_tokens_.IsEmpty());

    break_token_ = break_token;
  }

  // Return true if we broke inside this node on our own initiative (typically
  // not because of a child break, but rather due to the size of this node).
  bool DidBreakSelf() const { return did_break_self_; }
  void SetDidBreakSelf() { did_break_self_ = true; }

  // Store the previous break token, if one exists.
  void SetPreviousBreakToken(const NGBlockBreakToken* break_token) {
    previous_break_token_ = break_token;
  }
  const NGBlockBreakToken* PreviousBreakToken() const {
    return previous_break_token_;
  }

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
    if (last_inline_break_token_)
      return true;
    // Grid layout doesn't insert break before tokens, and instead set this bit
    // to indicate there is content after the current break.
    return has_subsequent_children_;
  }

  // Return true if we need to break before or inside any in-flow child that
  // doesn't establish a parallel flow. When this happens, we want to finish our
  // fragment, create a break token, and resume in the next fragmentainer.
  bool HasInflowChildBreakInside() const {
    return has_inflow_child_break_inside_;
  }

  // Return true if we need to break before or inside any floated child. Floats
  // are encapsulated by their container if the container establishes a new
  // block formatting context.
  bool HasFloatBreakInside() const { return has_float_break_inside_; }

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

  void SetInitialBreakBeforeIfNeeded(EBreakBetween break_before) {
    if (!initial_break_before_)
      initial_break_before_ = break_before;
  }

  void SetPreviousBreakAfter(EBreakBetween break_after) {
    previous_break_after_ = break_after;
  }

  // Join/"collapse" the previous (stored) break-after value with the next
  // break-before value, to determine how to deal with breaking between two
  // in-flow siblings.
  EBreakBetween JoinedBreakBetweenValue(EBreakBetween break_before) const;

  // Return the number of line boxes laid out.
  int LineCount() const { return line_count_; }

  // Set when we have iterated over all the children. This means that all
  // children have been fully laid out, or have break tokens. No more children
  // left to discover.
  void SetHasSeenAllChildren() { has_seen_all_children_ = true; }
  bool HasSeenAllChildren() { return has_seen_all_children_; }

  void SetHasSubsequentChildren() { has_subsequent_children_ = true; }

  void SetIsAtBlockEnd() { is_at_block_end_ = true; }
  bool IsAtBlockEnd() const { return is_at_block_end_; }

  void SetDisableOOFDescendantsPropagation() {
    disable_oof_descendants_propagation_ = true;
  }

  // See |NGPhysicalBoxFragment::InflowBounds|.
  void SetInflowBounds(const LogicalRect& inflow_bounds) {
    DCHECK_NE(box_type_, NGPhysicalBoxFragment::NGBoxType::kInlineBox);
    DCHECK(Node().IsScrollContainer());
#if DCHECK_IS_ON()
    is_inflow_bounds_explicitly_set_ = true;
#endif
    inflow_bounds_ = inflow_bounds;
  }

  void SetEarlyBreak(const NGEarlyBreak* breakpoint) {
    early_break_ = breakpoint;
  }
  bool HasEarlyBreak() const { return early_break_; }
  const NGEarlyBreak& EarlyBreak() const {
    DCHECK(early_break_);
    return *early_break_;
  }

  // Downgrade the break appeal if the specified break appeal is lower than any
  // found so far.
  void ClampBreakAppeal(NGBreakAppeal appeal) {
    break_appeal_ = std::min(break_appeal_, appeal);
  }

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

  NGPhysicalFragment::NGBoxType BoxType() const;
  void SetBoxType(NGPhysicalFragment::NGBoxType box_type) {
    box_type_ = box_type;
  }
  bool IsFragmentainerBoxType() const {
    return BoxType() == NGPhysicalFragment::kColumnBox;
  }
  void SetIsFieldsetContainer() { is_fieldset_container_ = true; }
  void SetIsTableNGPart() { is_table_ng_part_ = true; }
  void SetIsLegacyLayoutRoot() { is_legacy_layout_root_ = true; }

  void SetIsInlineFormattingContext(bool is_inline_formatting_context) {
    is_inline_formatting_context_ = is_inline_formatting_context;
  }

  void SetIsMathMLFraction() { is_math_fraction_ = true; }
  void SetIsMathMLOperator() { is_math_operator_ = true; }
  void SetMathMLPaintInfo(
      UChar operator_character,
      scoped_refptr<const ShapeResultView> operator_shape_result_view,
      LayoutUnit operator_inline_size,
      LayoutUnit operator_ascent,
      LayoutUnit operator_descent) {
    if (!mathml_paint_info_)
      mathml_paint_info_ = std::make_unique<NGMathMLPaintInfo>();

    mathml_paint_info_->operator_character = operator_character;
    mathml_paint_info_->operator_shape_result_view =
        std::move(operator_shape_result_view);

    mathml_paint_info_->operator_inline_size = operator_inline_size;
    mathml_paint_info_->operator_ascent = operator_ascent;
    mathml_paint_info_->operator_descent = operator_descent;
  }
  void SetMathMLPaintInfo(
      scoped_refptr<const ShapeResultView> operator_shape_result_view,
      LayoutUnit operator_inline_size,
      LayoutUnit operator_ascent,
      LayoutUnit operator_descent,
      LayoutUnit radical_operator_inline_offset,
      const NGBoxStrut& radical_base_margins) {
    if (!mathml_paint_info_)
      mathml_paint_info_ = std::make_unique<NGMathMLPaintInfo>();

    mathml_paint_info_->operator_character = kSquareRootCharacter;
    mathml_paint_info_->operator_shape_result_view =
        std::move(operator_shape_result_view);

    mathml_paint_info_->operator_inline_size = operator_inline_size;
    mathml_paint_info_->operator_ascent = operator_ascent;
    mathml_paint_info_->operator_descent = operator_descent;
    mathml_paint_info_->radical_base_margins = radical_base_margins;
    mathml_paint_info_->radical_operator_inline_offset =
        radical_operator_inline_offset;
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
  absl::optional<LayoutUnit> Baseline() const { return baseline_; }

  // Sets the last baseline for this fragment.
  void SetLastBaseline(LayoutUnit baseline) {
    DCHECK_EQ(space_->BaselineAlgorithmType(),
              NGBaselineAlgorithmType::kInlineBlock);
    last_baseline_ = baseline;
  }
  absl::optional<LayoutUnit> LastBaseline() const { return last_baseline_; }

  // The inline block baseline is at the block end margin edge under some
  // circumstances. This function updates |LastBaseline| in such cases.
  void SetLastBaselineToBlockEndMarginEdgeIfNeeded();

  void SetTableGridRect(const PhysicalRect& table_grid_rect) {
    table_grid_rect_ = table_grid_rect;
  }

  void SetTableColumnGeometries(
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

  void TransferGridLayoutData(
      std::unique_ptr<NGGridLayoutData> grid_layout_data) {
    grid_layout_data_ = std::move(grid_layout_data);
  }

  const NGGridLayoutData& GridLayoutData() const {
    DCHECK(grid_layout_data_);
    return *grid_layout_data_.get();
  }

  bool HasBreakTokenData() const { return break_token_data_.get(); }

  NGBlockBreakTokenData* EnsureBreakTokenData() {
    if (!HasBreakTokenData())
      break_token_data_ = std::make_unique<NGBlockBreakTokenData>();
    return break_token_data_.get();
  }

  NGBlockBreakTokenData* GetBreakTokenData() { return break_token_data_.get(); }

  void SetBreakTokenData(
      std::unique_ptr<NGBlockBreakTokenData> break_token_data) {
    break_token_data_ = std::move(break_token_data);
  }

  // The |NGFragmentItemsBuilder| for the inline formatting context of this box.
  NGFragmentItemsBuilder* ItemsBuilder() { return items_builder_; }
  void SetItemsBuilder(NGFragmentItemsBuilder* builder) {
    items_builder_ = builder;
  }

  // Returns offset for given child. DCHECK if child not found.
  // Warning: Do not call unless necessary.
  LogicalOffset GetChildOffset(const LayoutObject* child) const;

#if DCHECK_IS_ON()
  // If we don't participate in a fragmentation context, this method can check
  // that all block fragmentation related fields have their initial value.
  void CheckNoBlockFragmentation() const;
#endif

  // Moves all the children by |offset| in the block-direction. (Ensure that
  // any baselines, OOFs, etc, are also moved by the appropriate amount).
  void MoveChildrenInBlockDirection(LayoutUnit offset);

  void SetMathItalicCorrection(LayoutUnit italic_correction);

  void AdjustOffsetsForFragmentainerDescendant(
      NGLogicalOutOfFlowPositionedNode& descendant,
      bool only_fixedpos_containing_block = false);
  void AdjustFixedposContainingBlockForFragmentainerDescendants();
  void AdjustFixedposContainingBlockForInnerMulticols();

  // OOF positioned elements inside a fragmentation context are laid out once
  // they reach the fragmentation context root, so we need to adjust the offset
  // of its containing block to be relative to the fragmentation context
  // root. This allows us to determine the proper offset for the OOF inside the
  // same context. The block offset returned is the block contribution from
  // previous fragmentainers, if the current builder is a fragmentainer.
  // Otherwise, |fragmentainer_consumed_block_size| will be used. In some cases,
  // for example, we won't be able to calculate the adjustment from the builder.
  // This would happen when an OOF positioned element is nested inside another
  // OOF positioned element. The nested OOF will never have propagated up
  // through a fragmentainer builder. In such cases, the necessary adjustment
  // will be passed in via |fragmentainer_consumed_block_size|.
  LayoutUnit BlockOffsetAdjustmentForFragmentainer(
      LayoutUnit fragmentainer_consumed_block_size = LayoutUnit()) const;

  void SetHasForcedBreak() {
    has_forced_break_ = true;
    minimal_space_shortage_ = LayoutUnit::Max();
  }

 private:
  // Propagate fragmentation details. This includes checking whether we have
  // fragmented in this flow, break appeal, column spanner detection, and column
  // balancing hints.
  void PropagateBreakInfo(const NGLayoutResult&, LogicalOffset);

  scoped_refptr<const NGLayoutResult> ToBoxFragment(WritingMode);

  const NGFragmentGeometry* initial_fragment_geometry_ = nullptr;
  NGBoxStrut border_padding_;
  NGBoxStrut border_scrollbar_padding_;
  LogicalSize child_available_size_;
  LayoutUnit intrinsic_block_size_;
  absl::optional<LogicalRect> inflow_bounds_;

  NGFragmentItemsBuilder* items_builder_ = nullptr;

  NGPhysicalFragment::NGBoxType box_type_;
  bool is_fieldset_container_ = false;
  bool is_table_ng_part_ = false;
  bool is_initial_block_size_indefinite_ = false;
  bool is_inline_formatting_context_;
  bool is_known_to_fit_in_fragmentainer_ = false;
  bool requires_content_before_breaking_ = false;
  bool is_first_for_node_ = true;
  bool did_break_self_ = false;
  bool has_inflow_child_break_inside_ = false;
  bool has_float_break_inside_ = false;
  bool has_forced_break_ = false;
  bool is_new_fc_ = false;
  bool has_seen_all_children_ = false;
  bool has_subsequent_children_ = false;
  bool is_math_fraction_ = false;
  bool is_math_operator_ = false;
  bool is_at_block_end_ = false;
  bool disable_oof_descendants_propagation_ = false;
  LayoutUnit block_offset_for_additional_columns_;

  LayoutUnit minimal_space_shortage_ = LayoutUnit::Max();
  LayoutUnit tallest_unbreakable_block_size_ = LayoutUnit::Min();
  LayoutUnit block_size_for_fragmentation_;

  // The break-before value on the initial child we cannot honor. There's no
  // valid class A break point before a first child, only *between* siblings.
  absl::optional<EBreakBetween> initial_break_before_;

  // The break-after value of the previous in-flow sibling.
  EBreakBetween previous_break_after_ = EBreakBetween::kAuto;

  // The appeal of breaking inside this container.
  NGBreakAppeal break_appeal_ = kBreakAppealPerfect;

  absl::optional<LayoutUnit> baseline_;
  absl::optional<LayoutUnit> last_baseline_;

  // Table specific types.
  absl::optional<PhysicalRect> table_grid_rect_;
  absl::optional<NGTableFragmentData::ColumnGeometries>
      table_column_geometries_;
  scoped_refptr<const NGTableBorders> table_collapsed_borders_;
  std::unique_ptr<NGTableFragmentData::CollapsedBordersGeometry>
      table_collapsed_borders_geometry_;
  absl::optional<wtf_size_t> table_column_count_;

  // Table cell specific types.
  absl::optional<wtf_size_t> table_cell_column_index_;

  std::unique_ptr<NGBlockBreakTokenData> break_token_data_;

  // Grid specific types.
  std::unique_ptr<NGGridLayoutData> grid_layout_data_;

  LogicalBoxSides sides_to_include_;

  scoped_refptr<SerializedScriptValue> custom_layout_data_;

  std::unique_ptr<NGMathMLPaintInfo> mathml_paint_info_;
  absl::optional<NGLayoutResult::MathData> math_data_;

  const NGBlockBreakToken* previous_break_token_ = nullptr;

#if DCHECK_IS_ON()
  // Describes what size_.block_size represents; either the size of a single
  // fragment (false), or the size of all fragments for a node (true).
  bool block_size_is_for_all_fragments_ = false;

  // If any fragment has been added with an offset including the relative
  // position, we also need the inflow-bounds set explicitly.
  bool needs_inflow_bounds_explicitly_set_ = false;
  bool needs_may_have_descendant_above_block_start_explicitly_set_ = false;
  bool is_inflow_bounds_explicitly_set_ = false;
#endif

  friend class NGBlockBreakToken;
  friend class NGPhysicalBoxFragment;
  friend class NGLayoutResult;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_
