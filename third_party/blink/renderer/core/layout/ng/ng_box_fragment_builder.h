// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_border_edges.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
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
                       WritingMode writing_mode,
                       TextDirection direction)
      : NGContainerFragmentBuilder(node,
                                   std::move(style),
                                   space,
                                   writing_mode,
                                   direction),
        box_type_(NGPhysicalFragment::NGBoxType::kNormalBox),
        did_break_(false) {}

  // Build a fragment for LayoutObject without NGLayoutInputNode. LayoutInline
  // has NGInlineItem but does not have corresponding NGLayoutInputNode.
  NGBoxFragmentBuilder(LayoutObject* layout_object,
                       scoped_refptr<const ComputedStyle> style,
                       WritingMode writing_mode,
                       TextDirection direction)
      : NGContainerFragmentBuilder(/* node */ nullptr,
                                   std::move(style),
                                   /* space */ nullptr,
                                   writing_mode,
                                   direction),
        box_type_(NGPhysicalFragment::NGBoxType::kNormalBox),
        did_break_(false) {
    layout_object_ = layout_object;
  }

  void SetInitialFragmentGeometry(
      const NGFragmentGeometry& initial_fragment_geometry) {
    initial_fragment_geometry_ = &initial_fragment_geometry;
    size_ = initial_fragment_geometry_->border_box_size;
    is_initial_block_size_indefinite_ = size_.block_size == kIndefiniteSize;
  }

  const NGFragmentGeometry& InitialFragmentGeometry() const {
    DCHECK(initial_fragment_geometry_);
    return *initial_fragment_geometry_;
  }

  void SetUnconstrainedIntrinsicBlockSize(
      LayoutUnit unconstrained_intrinsic_block_size) {
    unconstrained_intrinsic_block_size_ = unconstrained_intrinsic_block_size;
  }
  void SetIntrinsicBlockSize(LayoutUnit intrinsic_block_size) {
    intrinsic_block_size_ = intrinsic_block_size;
  }
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

  // Add a break token for a child that doesn't yet have any fragments, because
  // its first fragment is to be produced in the next fragmentainer. This will
  // add a break token for the child, but no fragment.
  void AddBreakBeforeChild(NGLayoutInputNode child,
                           NGBreakAppeal,
                           bool is_forced_break);

  // Add a layout result. This involves appending the fragment and its relative
  // offset to the builder, but also keeping track of out-of-flow positioned
  // descendants, propagating fragmentainer breaks, and more.
  void AddResult(const NGLayoutResult&,
                 const LogicalOffset,
                 const LayoutInline* = nullptr);

  void AddBreakToken(scoped_refptr<const NGBreakToken>);

  void AddOutOfFlowLegacyCandidate(NGBlockNode,
                                   const NGLogicalStaticPosition&,
                                   const LayoutInline* inline_container);

  // Set how much of the block-size we've used so far for this box. This will be
  // the sum of the block-size of all previous fragments PLUS the one we're
  // building now.
  void SetConsumedBlockSize(LayoutUnit size) { consumed_block_size_ = size; }

  // Specify that we broke.
  //
  // This will result in a fragment which has an unfinished break token.
  void SetDidBreak() { did_break_ = true; }

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

  void SetColumnSpanner(NGBlockNode spanner) { column_spanner_ = spanner; }
  bool FoundColumnSpanner() const { return !!column_spanner_; }

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
  void SetIsFieldsetContainer() { is_fieldset_container_ = true; }
  void SetIsLegacyLayoutRoot() { is_legacy_layout_root_ = true; }

  bool DidBreak() const { return did_break_; }

  void SetBorderEdges(NGBorderEdges border_edges) {
    border_edges_ = border_edges;
  }

  // Either this function or SetBoxType must be called before ToBoxFragment().
  void SetIsNewFormattingContext(bool is_new_fc) { is_new_fc_ = is_new_fc; }

  void SetCustomLayoutData(
      scoped_refptr<SerializedScriptValue> custom_layout_data) {
    custom_layout_data_ = std::move(custom_layout_data);
  }

  // Layout algorithms should call this function for each baseline request in
  // the constraint space.
  //
  // If a request should use a synthesized baseline from the box rectangle,
  // algorithms can omit the call.
  //
  // This function should be called at most once for a given algorithm/baseline
  // type pair.
  void AddBaseline(NGBaselineRequest, LayoutUnit);

  // The |NGFragmentItemsBuilder| for the inline formatting context of this box.
  NGFragmentItemsBuilder* ItemsBuilder() { return items_builder_; }
  void SetItemsBuilder(NGFragmentItemsBuilder* builder) {
    items_builder_ = builder;
  }

  // Inline containing block geometry is defined by two rectangles defined
  // by fragments generated by LayoutInline.
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
  void ComputeInlineContainerFragments(
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
    minimal_space_shortage_ = LayoutUnit();
  }

  scoped_refptr<const NGLayoutResult> ToBoxFragment(WritingMode);

  const NGFragmentGeometry* initial_fragment_geometry_ = nullptr;
  LayoutUnit unconstrained_intrinsic_block_size_ = kIndefiniteSize;
  LayoutUnit intrinsic_block_size_;

  NGFragmentItemsBuilder* items_builder_ = nullptr;

  NGBlockNode column_spanner_ = nullptr;

  NGPhysicalFragment::NGBoxType box_type_;
  bool is_fieldset_container_ = false;
  bool is_initial_block_size_indefinite_ = false;
  bool did_break_;
  bool has_forced_break_ = false;
  bool is_new_fc_ = false;
  bool subtree_modified_margin_strut_ = false;
  bool has_seen_all_children_ = false;
  LayoutUnit consumed_block_size_;

  LayoutUnit minimal_space_shortage_ = LayoutUnit::Max();
  LayoutUnit tallest_unbreakable_block_size_ = LayoutUnit::Min();

  // The break-before value on the initial child we cannot honor. There's no
  // valid class A break point before a first child, only *between* siblings.
  EBreakBetween initial_break_before_ = EBreakBetween::kAuto;

  // The break-after value of the previous in-flow sibling.
  EBreakBetween previous_break_after_ = EBreakBetween::kAuto;

  NGBaselineList baselines_;

  NGBorderEdges border_edges_;

  scoped_refptr<SerializedScriptValue> custom_layout_data_;

  friend class NGPhysicalBoxFragment;
  friend class NGLayoutResult;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_
