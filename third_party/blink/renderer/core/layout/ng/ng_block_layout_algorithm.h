// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockLayoutAlgorithm_h
#define NGBlockLayoutAlgorithm_h

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float_vector.h"

namespace blink {

class NGConstraintSpace;
class NGFragment;
class NGLayoutResult;
class NGPhysicalLineBoxFragment;

// This struct is used for communicating to a child the position of the previous
// inflow child. This will be used to calculate the position of the next child.
struct NGPreviousInflowPosition {
  LayoutUnit logical_block_offset;
  NGMarginStrut margin_strut;
  bool empty_block_affected_by_clearance;
};

// This strut holds information for the current inflow child. The data is not
// useful outside of handling this single inflow child.
struct NGInflowChildData {
  NGBfcOffset bfc_offset_estimate;
  NGMarginStrut margin_strut;
  NGBoxStrut margins;
  bool margins_fully_resolved;
  bool force_clearance;
  bool is_new_fc;
};

// A class for general block layout (e.g. a <div> with no special style).
// Lays out the children in sequence.
class CORE_EXPORT NGBlockLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  // Default constructor.
  // @param node The input node to perform layout upon.
  // @param space The constraint space which the algorithm should generate a
  //              fragment within.
  // @param break_token The break token from which the layout should start.
  NGBlockLayoutAlgorithm(NGBlockNode node,
                         const NGConstraintSpace& space,
                         const NGBlockBreakToken* break_token = nullptr);

  ~NGBlockLayoutAlgorithm() override;

  void SetBoxType(NGPhysicalFragment::NGBoxType type);

  base::Optional<MinMaxSize> ComputeMinMaxSize(
      const MinMaxSizeInput&) const override;
  scoped_refptr<NGLayoutResult> Layout() override;

 private:
  // Return the BFC block offset of this block.
  LayoutUnit BfcBlockOffset() const {
    // If we have resolved our BFC block offset, use that.
    if (container_builder_.BfcBlockOffset())
      return *container_builder_.BfcBlockOffset();
    // Otherwise fall back to the BFC block offset assigned by the parent
    // algorithm.
    return ConstraintSpace().BfcOffset().block_offset;
  }

  // Return the BFC block offset of the next block-start border edge (for some
  // child) we'd get if we commit pending margins.
  LayoutUnit NextBorderEdge(
      const NGPreviousInflowPosition& previous_inflow_position) const {
    return BfcBlockOffset() + previous_inflow_position.logical_block_offset +
           previous_inflow_position.margin_strut.Sum();
  }

  NGBoxStrut CalculateMargins(NGLayoutInputNode child,
                              bool is_new_fc,
                              const NGBreakToken* child_break_token,
                              bool* margins_fully_resolved);

  // Creates a new constraint space for the current child.
  NGConstraintSpace CreateConstraintSpaceForChild(
      const NGLayoutInputNode child,
      const NGInflowChildData& child_data,
      const NGLogicalSize child_available_size,
      const base::Optional<LayoutUnit> floats_bfc_block_offset = base::nullopt);

  // @return Estimated BFC block offset for the "to be layout" child.
  NGInflowChildData ComputeChildData(const NGPreviousInflowPosition&,
                                     NGLayoutInputNode,
                                     const NGBreakToken* child_break_token,
                                     bool force_clearance,
                                     bool is_new_fc);

  NGPreviousInflowPosition ComputeInflowPosition(
      const NGPreviousInflowPosition&,
      const NGLayoutInputNode child,
      const NGInflowChildData&,
      const base::Optional<LayoutUnit>& child_bfc_block_offset,
      const NGLogicalOffset&,
      const NGLayoutResult&,
      const NGFragment&,
      bool empty_block_affected_by_clearance);

  // Position an empty child using the parent BFC block offset.
  // The fragment doesn't know its offset, but we can still calculate its BFC
  // position because the parent fragment's BFC is known.
  // Example:
  //   BFC Offset is known here because of the padding.
  //   <div style="padding: 1px">
  //     <div id="empty-div" style="margin: 1px"></div>
  LayoutUnit PositionEmptyChildWithParentBfc(
      const NGLayoutInputNode& child,
      const NGConstraintSpace& child_space,
      const NGInflowChildData& child_data,
      const NGLayoutResult&) const;

  void HandleOutOfFlowPositioned(const NGPreviousInflowPosition&, NGBlockNode);
  void HandleFloat(const NGPreviousInflowPosition&,
                   NGBlockNode,
                   const NGBlockBreakToken*);

  // This uses the NGLayoutOpporunityIterator to position the fragment.
  //
  // An element that establishes a new formatting context must not overlap the
  // margin box of any floats within the current BFC.
  //
  // Example:
  // <div id="container">
  //   <div id="float"></div>
  //   <div id="new-fc" style="margin-top: 20px;"></div>
  // </div>
  // 1) If #new-fc is small enough to fit the available space right from #float
  //    then it will be placed there and we collapse its margin.
  // 2) If #new-fc is too big then we need to clear its position and place it
  //    below #float ignoring its vertical margin.
  //
  // Returns false if we need to abort layout, because a previously unknown BFC
  // block offset has now been resolved.
  bool HandleNewFormattingContext(
      NGLayoutInputNode child,
      const NGBreakToken* child_break_token,
      NGPreviousInflowPosition*,
      scoped_refptr<const NGBreakToken>* previous_inline_break_token);

  // Performs the actual layout of a new formatting context. This may be called
  // multiple times from HandleNewFormattingContext.
  std::pair<scoped_refptr<NGLayoutResult>, NGLayoutOpportunity>
  LayoutNewFormattingContext(NGLayoutInputNode child,
                             const NGBreakToken* child_break_token,
                             const NGInflowChildData&,
                             NGBfcOffset origin_offset,
                             bool abort_if_cleared);

  // Handle an in-flow child.
  // Returns false if we need to abort layout, because a previously unknown BFC
  // block offset has now been resolved. (Same as HandleNewFormattingContext).
  bool HandleInflow(
      NGLayoutInputNode child,
      const NGBreakToken* child_break_token,
      NGPreviousInflowPosition*,
      scoped_refptr<const NGBreakToken>* previous_inline_break_token);

  // Return the amount of block space available in the current fragmentainer
  // for the node being laid out by this algorithm.
  LayoutUnit FragmentainerSpaceAvailable() const;

  // Return true if the node being laid out by this fragmentainer has used all
  // the available space in the current fragmentainer.
  // |block_offset| is the border-edge relative block offset we want to check
  // whether fits within the fragmentainer or not.
  bool IsFragmentainerOutOfSpace(LayoutUnit block_offset) const;

  // Insert a fragmentainer break before the child if necessary.
  // Update previous in-flow position and return true if a break was inserted.
  // Otherwise return false.
  bool BreakBeforeChild(NGLayoutInputNode child,
                        const NGLayoutResult&,
                        NGPreviousInflowPosition*,
                        LayoutUnit block_offset,
                        bool is_pushed_by_floats);

  enum BreakType { NoBreak, SoftBreak, ForcedBreak };

  // Given a child fragment and the corresponding node's style, determine the
  // type of break we should insert in front of it, if any.
  BreakType BreakTypeBeforeChild(NGLayoutInputNode child,
                                 const NGLayoutResult&,
                                 LayoutUnit block_offset,
                                 bool is_pushed_by_floats) const;

  // Final adjustments before fragment creation. We need to prevent the
  // fragment from crossing fragmentainer boundaries, and rather create a break
  // token if we're out of space.
  void FinalizeForFragmentation();

  void PropagateBaselinesFromChildren();
  bool AddBaseline(const NGBaselineRequest&,
                   const NGPhysicalFragment*,
                   LayoutUnit child_offset);

  // Compute the baseline offset of a line box from the content box.
  // Line boxes are in line-relative coordinates. This function returns the
  // offset in flow-relative coordinates.
  LayoutUnit ComputeLineBoxBaselineOffset(
      const NGBaselineRequest&,
      const NGPhysicalLineBoxFragment&,
      LayoutUnit line_box_block_offset) const;

  // If still unresolved, resolve the fragment's BFC block offset.
  //
  // This includes applying clearance, so the bfc_block_offset passed won't be
  // the final BFC block offset, if it wasn't large enough to get past all
  // relevant floats. The updated BFC block offset can be read out with
  // ContainerBfcBlockOffset().
  //
  // In addition to resolving our BFC block offset, this will also position
  // pending floats, and update our in-flow layout state. Returns false if
  // resolving the BFC block offset resulted in needing to abort layout. It
  // will always return true otherwise. If the BFC block offset was already
  // resolved, this method does nothing (and returns true).
  bool ResolveBfcBlockOffset(NGPreviousInflowPosition*,
                             LayoutUnit bfc_block_offset);

  // A very common way to resolve the BFC block offset is to simply commit the
  // pending margin, so here's a convenience overload for that.
  bool ResolveBfcBlockOffset(
      NGPreviousInflowPosition* previous_inflow_position) {
    return ResolveBfcBlockOffset(previous_inflow_position,
                                 NextBorderEdge(*previous_inflow_position));
  }

  // Return true if the BFC block offset has changed and this means that we
  // need to abort layout.
  bool NeedsAbortOnBfcBlockOffsetChange() const;

  // Positions pending floats starting from {@origin_block_offset}.
  void PositionPendingFloats(LayoutUnit origin_block_offset);

  // Adds a set of positioned floats as children to the current fragment.
  template <class Vec>
  void AddPositionedFloats(const Vec& positioned_floats);

  // Positions a list marker for the specified block content.
  void PositionOrPropagateListMarker(const NGLayoutResult&, NGLogicalOffset*);

  // Positions a list marker when the block does not have any line boxes.
  void PositionListMarkerWithoutLineBoxes();

  // Calculates logical offset for the current fragment using either {@code
  // intrinsic_block_size_} when the fragment doesn't know it's offset or
  // {@code known_fragment_offset} if the fragment knows it's offset
  // @return Fragment's offset relative to the fragment's parent.
  NGLogicalOffset CalculateLogicalOffset(
      const NGFragment& fragment,
      LayoutUnit child_bfc_line_offset,
      const base::Optional<LayoutUnit>& child_bfc_block_offset);

  // Computes default content size for HTML and BODY elements in quirks mode.
  // Returns NGSizeIndefinite in all other cases.
  LayoutUnit CalculateDefaultBlockSize();

  // Computes minimum size for HTML and BODY elements in quirks mode.
  // Returns NGSizeIndefinite in all other cases.
  LayoutUnit CalculateMinimumBlockSize(const NGMarginStrut& end_margin_strut);

  NGLogicalSize child_available_size_;
  NGLogicalSize child_percentage_size_;
  NGLogicalSize replaced_child_percentage_size_;

  NGBoxStrut border_padding_;
  NGBoxStrut border_scrollbar_padding_;
  LayoutUnit intrinsic_block_size_;

  NGInlineChildLayoutContext inline_child_layout_context_;

  // The line box index at which we ran out of space. This where we'll actually
  // end up breaking, unless we determine that we should break earlier in order
  // to satisfy the widows request.
  int first_overflowing_line_ = 0;

  // Set if we should fit as many lines as there's room for, i.e. no early
  // break. In that case we'll break before first_overflowing_line_. In this
  // case there'll either be enough widows for the next fragment, or we have
  // determined that we're unable to fulfill the widows request.
  bool fit_all_lines_ = false;

  // Set if we're resuming layout of a node that has already produced fragments.
  bool is_resuming_;

  // Set when we're to abort if the BFC block offset gets resolved or updated.
  // Sometimes we walk past elements (i.e. floats) that depend on the BFC block
  // offset being known (in order to position and lay themselves out properly).
  // When this happens, and we finally manage to resolve (or update) the BFC
  // block offset at some subsequent element, we need to check if this flag is
  // set, and abort layout if it is.
  bool abort_when_bfc_block_offset_updated_ = false;

  bool has_processed_first_child_ = false;

  NGExclusionSpace exclusion_space_;
  NGUnpositionedFloatVector unpositioned_floats_;
};

}  // namespace blink

#endif  // NGBlockLayoutAlgorithm_h
