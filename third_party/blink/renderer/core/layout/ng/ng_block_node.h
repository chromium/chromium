// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class LayoutBox;
class NGBlockBreakToken;
class NGBoxFragmentBuilder;
class NGConstraintSpace;
class NGEarlyBreak;
class NGLayoutResult;
class NGPhysicalBoxFragment;
class NGPhysicalContainerFragment;
struct NGBoxStrut;
struct NGLayoutAlgorithmParams;

enum class MathScriptType;

// Represents a node to be laid out.
class CORE_EXPORT NGBlockNode final : public NGLayoutInputNode {
  friend NGLayoutInputNode;
 public:
  explicit NGBlockNode(LayoutBox* box) : NGLayoutInputNode(box, kBlock) {}

  NGBlockNode(std::nullptr_t) : NGLayoutInputNode(nullptr) {}

  scoped_refptr<const NGLayoutResult> Layout(
      const NGConstraintSpace& constraint_space,
      const NGBlockBreakToken* break_token = nullptr,
      const NGEarlyBreak* = nullptr) const;

  // This method is just for use within the |NGSimplifiedLayoutAlgorithm|.
  //
  // If layout is dirty, it will perform layout using the previous constraint
  // space used to generate the |NGLayoutResult|.
  // Otherwise it will simply return the previous layout result generated.
  scoped_refptr<const NGLayoutResult> SimplifiedLayout(
      const NGPhysicalFragment& previous_fragment) const;

  // This method is just for use within the |NGOutOfFlowLayoutPart|.
  //
  // As OOF-positioned objects have their position, and size computed
  // pre-layout, we need a way to quickly determine if we need to perform this
  // work.
  //
  // We have this "first-tier" cache explicitly for this purpose.
  // This method compares the containing-block size to determine if we can skip
  // the position, and size calculation.
  //
  // If the containing-block size hasn't changed, and we are layout-clean we
  // can reuse the previous layout result.
  scoped_refptr<const NGLayoutResult> CachedLayoutResultForOutOfFlowPositioned(
      LogicalSize container_content_size) const;

  NGLayoutInputNode NextSibling() const;

  // Computes the value of min-content and max-content for this node's border
  // box.
  // If the underlying layout algorithm's ComputeMinMaxSizes returns
  // no value, this function will synthesize these sizes using Layout with
  // special constraint spaces -- infinite available size for max content, zero
  // available size for min content, and percentage resolution size zero for
  // both.
  // An optional constraint space may be supplied, which will be used to resolve
  // percentage padding on this node, to set up the right min/max size
  // contribution. This is typically desirable for the subtree root of the
  // min/max calculation (e.g. the node that will undergo shrink-to-fit). It is
  // also used to provide provide a sensible available inline size when
  // calculating min/max for orthogonal flows. This constraint space will not be
  // passed on to children. If no constraint space is specified, a zero-sized
  // one will be used.
  // The constraint space is also used to perform layout when this block's
  // writing mode is orthogonal to its parent's, in which case the constraint
  // space is not optional.
  MinMaxSizesResult ComputeMinMaxSizes(
      WritingMode container_writing_mode,
      const MinMaxSizesInput&,
      const NGConstraintSpace* = nullptr) const;

  MinMaxSizes ComputeMinMaxSizesFromLegacy(const MinMaxSizesInput&) const;

  NGLayoutInputNode FirstChild() const;

  NGBlockNode GetRenderedLegend() const;
  NGBlockNode GetFieldsetContent() const;

  bool IsNGTable() const { return IsTable() && box_->IsLayoutNGMixin(); }
  bool IsNGTableCell() const {
    return box_->IsTableCell() && !box_->IsTableCellLegacy();
  }

  bool IsFixedTableLayout() const;
  const NGBoxStrut& GetTableBorders() const;
  LayoutUnit ComputeTableInlineSize(const NGConstraintSpace&,
                                    const NGBoxStrut& border_padding) const;

  // Return true if this block node establishes an inline formatting context.
  // This will only be the case if there is actual inline content. Empty nodes
  // or nodes consisting purely of block-level, floats, and/or out-of-flow
  // positioned children will return false.
  bool IsInlineFormattingContextRoot(
      NGLayoutInputNode* first_child_out = nullptr) const;

  bool IsInlineLevel() const;
  bool IsAtomicInlineLevel() const;
  bool HasAspectRatio() const;

  // Returns the aspect ratio of a replaced element.
  LogicalSize GetAspectRatio() const;

  // Returns the transform to apply to a child (e.g. for layout-overflow).
  base::Optional<TransformationMatrix> GetTransformForChildFragment(
      const NGPhysicalBoxFragment& child_fragment,
      PhysicalSize size) const;

  bool HasLeftOverflow() const { return box_->HasLeftOverflow(); }
  bool HasTopOverflow() const { return box_->HasTopOverflow(); }
  bool HasNonVisibleOverflow() const { return box_->HasNonVisibleOverflow(); }

  OverflowClipAxes GetOverflowClipAxes() const {
    return box_->GetOverflowClipAxes();
  }

  // Returns true if this node should fill the viewport.
  // This occurs when we are in quirks-mode and we are *not* OOF-positioned,
  // floating, or inline-level.
  //
  // https://quirks.spec.whatwg.org/#the-body-element-fills-the-html-element-quirk
  bool IsQuirkyAndFillsViewport() const {
    if (!GetDocument().InQuirksMode())
      return false;
    if (IsOutOfFlowPositioned())
      return false;
    if (IsFloating())
      return false;
    if (IsAtomicInlineLevel())
      return false;
    return (IsDocumentElement() || IsBody());
  }

  // Returns true if the custom layout node is in its loaded state (all script
  // for the web-developer defined layout is ready).
  bool IsCustomLayoutLoaded() const;

  // Get script type for scripts (msub, msup, msubsup, munder, mover and
  // munderover).
  MathScriptType ScriptType() const;

  // Find out if the radical has an index.
  bool HasIndex() const;

  // Layout an atomic inline; e.g., inline block.
  scoped_refptr<const NGLayoutResult> LayoutAtomicInline(
      const NGConstraintSpace& parent_constraint_space,
      const ComputedStyle& parent_style,
      bool use_first_line_style,
      NGBaselineAlgorithmType baseline_algorithm_type =
          NGBaselineAlgorithmType::kInlineBlock);

  // Called if this is an out-of-flow block which needs to be
  // positioned with legacy layout.
  void UseLegacyOutOfFlowPositioning() const;

  // Write back resolved margins to legacy.
  void StoreMargins(const NGConstraintSpace&, const NGBoxStrut& margins);
  void StoreMargins(const NGPhysicalBoxStrut& margins);

  // Add a column layout result to a list. Columns are essentially
  // LayoutObject-less, but we still need to keep the fragments generated
  // somewhere.
  void AddColumnResult(scoped_refptr<const NGLayoutResult>,
                       const NGBlockBreakToken* incoming_break_token) const;

  static bool CanUseNewLayout(const LayoutBox&);
  bool CanUseNewLayout() const;

  bool ShouldApplyLayoutContainment() const {
    return box_->ShouldApplyLayoutContainment();
  }

  bool HasLineIfEmpty() const {
    if (const auto* block = DynamicTo<LayoutBlock>(box_))
      return block->HasLineIfEmpty();
    return false;
  }

  String ToString() const;

 private:
  void PrepareForLayout() const;

  // Runs layout on the underlying LayoutObject and creates a fragment for the
  // resulting geometry.
  scoped_refptr<const NGLayoutResult> RunLegacyLayout(
      const NGConstraintSpace&) const;

  scoped_refptr<const NGLayoutResult> RunSimplifiedLayout(
      const NGLayoutAlgorithmParams&,
      const NGLayoutResult&) const;

  // If this node is a LayoutNGMixin, the caller must pass the layout object for
  // this node cast to a LayoutBlockFlow as the first argument.
  void FinishLayout(LayoutBlockFlow*,
                    const NGConstraintSpace&,
                    const NGBlockBreakToken*,
                    scoped_refptr<const NGLayoutResult>) const;

  // After we run the layout algorithm, this function copies back the geometry
  // data to the layout box.
  void CopyFragmentDataToLayoutBox(
      const NGConstraintSpace&,
      const NGLayoutResult&,
      const NGBlockBreakToken* previous_break_token) const;
  void CopyFragmentItemsToLayoutBox(
      const NGPhysicalBoxFragment& container,
      const NGFragmentItems& items,
      const NGBlockBreakToken* previous_break_token) const;
  void CopyFragmentDataToLayoutBoxForInlineChildren(
      const NGPhysicalContainerFragment& container,
      LayoutUnit initial_container_width,
      bool initial_container_is_flipped,
      PhysicalOffset offset = {}) const;
  void PlaceChildrenInLayoutBox(
      const NGPhysicalBoxFragment&,
      const NGBlockBreakToken* previous_break_token) const;
  void PlaceChildrenInFlowThread(
      LayoutMultiColumnFlowThread*,
      const NGConstraintSpace&,
      const NGPhysicalBoxFragment&,
      const NGBlockBreakToken* previous_container_break_token) const;
  void CopyChildFragmentPosition(
      const NGPhysicalBoxFragment& child_fragment,
      PhysicalOffset,
      const NGPhysicalBoxFragment& container_fragment,
      const NGBlockBreakToken* previous_container_break_token = nullptr) const;

  void CopyBaselinesFromLegacyLayout(const NGConstraintSpace&,
                                     NGBoxFragmentBuilder*) const;
  LayoutUnit AtomicInlineBaselineFromLegacyLayout(
      const NGConstraintSpace&) const;

  void UpdateShapeOutsideInfoIfNeeded(
      const NGLayoutResult&,
      LayoutUnit percentage_resolution_inline_size) const;
};

template <>
struct DowncastTraits<NGBlockNode> {
  static bool AllowFrom(const NGLayoutInputNode& node) {
    return node.IsBlock();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_NODE_H_
