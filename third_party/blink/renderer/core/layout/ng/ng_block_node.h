// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_NODE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class LayoutBox;
class NGBlockBreakToken;
class NGColumnSpannerPath;
class NGConstraintSpace;
class NGEarlyBreak;
class NGFragmentItems;
class NGLayoutResult;
class NGPhysicalBoxFragment;
class NGPhysicalFragment;
struct NGBoxStrut;
struct NGLayoutAlgorithmParams;

enum class MathScriptType;

// Represents a node to be laid out.
class CORE_EXPORT NGBlockNode : public NGLayoutInputNode {
  friend NGLayoutInputNode;
 public:
  explicit NGBlockNode(LayoutBox* box) : NGLayoutInputNode(box, kBlock) {}

  NGBlockNode(std::nullptr_t) : NGLayoutInputNode(nullptr) {}

  const NGLayoutResult* Layout(const NGConstraintSpace& constraint_space,
                               const NGBlockBreakToken* break_token = nullptr,
                               const NGEarlyBreak* = nullptr,
                               const NGColumnSpannerPath* = nullptr) const;

  // This method is just for use within the |NGSimplifiedLayoutAlgorithm|.
  //
  // If layout is dirty, it will perform layout using the previous constraint
  // space used to generate the |NGLayoutResult|.
  // Otherwise it will simply return the previous layout result generated.
  const NGLayoutResult* SimplifiedLayout(
      const NGPhysicalFragment& previous_fragment) const;

  // Lay out a repeatable node during block fragmentation (fixed positioned
  // element during printing, or table header / footer). To be called once for
  // each container fragment in which it repeats.
  //
  // NGConstraintSpace::ShouldRepeat() will tell whether the node is
  // (potentially [1]) going to repeat again (in which case an outgoing "repeat"
  // break token will be created, or if this is the last time.
  // FinishRepeatableRoot() will be invoked if it's the last time. It is allowed
  // to call this function with NGConstraintSpace::ShouldRepeat() set to true
  // every time, but then the calling code needs to call FinishRepeatableRoot()
  // when it realizes that we're done.
  //
  // [1] Depending on the type of content, and depending on the way we implement
  // it, we may or may not be able to tell up-front whether it's going to repeat
  // again.
  //
  // Note that we only actually lay it out once - when at the first container
  // fragment. Any subsequent call will just clone the previous result.
  //
  // Ideally, there should only be one fragment subtree generated from a
  // repeated element (which could simply be inserted inside every relevant
  // container fragment), but due to requirements from pre-paint and paint
  // (mainly), we need to clone the fragment as many times as it repeats, and we
  // also need to make sure that the break tokens are reasonably intact -
  // including the sequence numbers. This is why we need this.
  const NGLayoutResult* LayoutRepeatableRoot(const NGConstraintSpace&,
                                             const NGBlockBreakToken*) const;

  // Finalize the cloned layout results of a repeatable root. This will
  // deep-clone and set the correct break token sequence numbers, and make sure
  // that the final fragment has no outgoing break token.
  //
  // To be called when we're done repeating a node, when at the last fragment.
  void FinishRepeatableRoot() const;

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
  const NGLayoutResult* CachedLayoutResultForOutOfFlowPositioned(
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
      const MinMaxSizesType,
      const NGConstraintSpace&,
      const MinMaxSizesFloatInput float_input = MinMaxSizesFloatInput()) const;

  MinMaxSizes ComputeMinMaxSizesFromLegacy(const MinMaxSizesType,
                                           const NGConstraintSpace&) const;

  NGLayoutInputNode FirstChild() const;

  NGBlockNode GetRenderedLegend() const;
  NGBlockNode GetFieldsetContent() const;

  bool IsNGTableCell() const { return box_->IsTableCell(); }

  bool IsContainingBlockNGGrid() const {
    return box_->ContainingBlock()->IsLayoutNGGrid();
  }
  bool IsFrameSet() const { return box_->IsFrameSet(); }
  bool IsParentNGFrameSet() const { return box_->Parent()->IsFrameSet(); }
  bool IsParentNGGrid() const { return box_->Parent()->IsLayoutNGGrid(); }

  // Return true if this block node establishes an inline formatting context.
  // This will only be the case if there is actual inline content. Empty nodes
  // or nodes consisting purely of block-level, floats, and/or out-of-flow
  // positioned children will return false.
  bool IsInlineFormattingContextRoot(
      NGLayoutInputNode* first_child_out = nullptr) const;

  bool IsInlineLevel() const;
  bool IsAtomicInlineLevel() const;
  bool HasAspectRatio() const;
  bool IsInTopOrViewTransitionLayer() const;

  // Returns the aspect ratio of a replaced element.
  LogicalSize GetAspectRatio() const;

  // SVG roots sometimes have sizing peculiarities that override regular sizing.
  // Returns {0,0} if there's no override.
  LogicalSize GetReplacedSizeOverrideIfAny(const NGConstraintSpace&) const;

  // Returns the transform to apply to a child (e.g. for layout-overflow).
  absl::optional<gfx::Transform> GetTransformForChildFragment(
      const NGPhysicalBoxFragment& child_fragment,
      PhysicalSize size) const;

  bool HasLeftOverflow() const { return box_->HasLeftOverflow(); }
  bool HasTopOverflow() const { return box_->HasTopOverflow(); }
  bool HasNonVisibleOverflow() const { return box_->HasNonVisibleOverflow(); }

  // Return true if overflow in the block direction is clipped. With
  // overflow-[xy]:clip, it is possible with visible overflow along one axis at
  // the same time as we clip it along the other axis.
  bool HasNonVisibleBlockOverflow() const;

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
  const NGLayoutResult* LayoutAtomicInline(
      const NGConstraintSpace& parent_constraint_space,
      const ComputedStyle& parent_style,
      bool use_first_line_style,
      NGBaselineAlgorithmType baseline_algorithm_type);

  void InsertIntoLegacyPositionedObjectsOf(LayoutBlock*) const;

  // Write back resolved margins to legacy.
  void StoreMargins(const NGConstraintSpace&, const NGBoxStrut& margins);
  void StoreMargins(const NGPhysicalBoxStrut& margins);

  // Write the inline-size and number of columns in a multicol container to
  // legacy.
  void StoreColumnSizeAndCount(LayoutUnit inline_size, int count);

  static bool CanUseNewLayout(const LayoutBox&);
  bool CanUseNewLayout() const;

  bool ShouldApplyLayoutContainment() const {
    return box_->ShouldApplyLayoutContainment();
  }

  bool ShouldApplyPaintContainment() const {
    return box_->ShouldApplyPaintContainment();
  }

  bool HasLineIfEmpty() const {
    if (const auto* block = DynamicTo<LayoutBlock>(box_.Get()))
      return block->HasLineIfEmpty();
    return false;
  }
  LayoutUnit EmptyLineBlockSize(
      const NGBlockBreakToken* incoming_break_token) const;

  // After we run the layout algorithm, this function copies back the fragment
  // position to the layout box.
  void CopyChildFragmentPosition(
      const NGPhysicalBoxFragment& child_fragment,
      PhysicalOffset,
      const NGPhysicalBoxFragment& container_fragment,
      const NGBlockBreakToken* previous_container_break_token = nullptr,
      bool needs_invalidation_check = false) const;

  // If extra columns are added after a multicol has been written back to
  // legacy, for example for an OOF positioned element, we need to update the
  // legacy flow thread to encompass those extra columns.
  void MakeRoomForExtraColumns(LayoutUnit block_size) const;

  String ToString() const;

 private:
  void PrepareForLayout() const;

  const NGLayoutResult* RunSimplifiedLayout(const NGLayoutAlgorithmParams&,
                                            const NGLayoutResult&) const;

  // If this node is a LayoutNGMixin, the caller must pass the layout object for
  // this node cast to a LayoutBlockFlow as the first argument.
  void FinishLayout(LayoutBlockFlow*,
                    const NGConstraintSpace&,
                    const NGBlockBreakToken*,
                    const NGLayoutResult*,
                    LayoutSize old_box_size) const;

  // Update the layout results vector in LayoutBox with the new result.
  void StoreResultInLayoutBox(const NGLayoutResult*,
                              const NGBlockBreakToken*,
                              bool clear_trailing_results = false) const;

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
  void PlaceChildrenInLayoutBox(const NGPhysicalBoxFragment&,
                                const NGBlockBreakToken* previous_break_token,
                                bool needs_invalidation_check = false) const;
  void PlaceChildrenInFlowThread(
      LayoutMultiColumnFlowThread*,
      const NGConstraintSpace&,
      const NGPhysicalBoxFragment&,
      const NGBlockBreakToken* previous_container_break_token) const;

  void UpdateMarginPaddingInfoIfNeeded(const NGConstraintSpace&) const;

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

// Devtools can trigger layout to collect devtools-specific data. We don't want
// or need such devtools layouts to write to the fragment or layout trees. This
// class sets a flag that is checked before storing the layout results. If the
// flag is true, we bail before writing anything.
class DevtoolsReadonlyLayoutScope {
  STACK_ALLOCATED();

 public:
  DevtoolsReadonlyLayoutScope();
  static bool InDevtoolsLayout();
  ~DevtoolsReadonlyLayoutScope();
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NGBlockNode)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_NODE_H_
