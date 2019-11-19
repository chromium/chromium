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
class NGBaselineRequest;
class NGBlockBreakToken;
class NGBoxFragmentBuilder;
class NGBreakToken;
class NGConstraintSpace;
class NGEarlyBreak;
class NGLayoutResult;
class NGPhysicalBoxFragment;
class NGPhysicalContainerFragment;
class NGPhysicalFragment;
struct MinMaxSize;
struct NGBoxStrut;
struct NGLayoutAlgorithmParams;
struct LogicalOffset;

// Represents a node to be laid out.
class CORE_EXPORT NGBlockNode final : public NGLayoutInputNode {
  friend NGLayoutInputNode;
 public:
  explicit NGBlockNode(LayoutBox* box) : NGLayoutInputNode(box, kBlock) {}

  NGBlockNode(std::nullptr_t) : NGLayoutInputNode(nullptr) {}

  scoped_refptr<const NGLayoutResult> Layout(
      const NGConstraintSpace& constraint_space,
      const NGBreakToken* break_token = nullptr,
      const NGEarlyBreak* = nullptr);

  // This method is just for use within the |NGSimplifiedLayoutAlgorithm|.
  //
  // If layout is dirty, it will perform layout using the previous constraint
  // space used to generate the |NGLayoutResult|.
  // Otherwise it will simply return the previous layout result generated.
  scoped_refptr<const NGLayoutResult> SimplifiedLayout();

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
  // If the underlying layout algorithm's ComputeMinMaxSize returns
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
  MinMaxSize ComputeMinMaxSize(WritingMode container_writing_mode,
                               const MinMaxSizeInput&,
                               const NGConstraintSpace* = nullptr);

  MinMaxSize ComputeMinMaxSizeFromLegacy(const MinMaxSizeInput&) const;

  NGLayoutInputNode FirstChild() const;

  NGBlockNode GetRenderedLegend() const;
  NGBlockNode GetFieldsetContent() const;

  bool ChildrenInline() const;
  bool IsInlineLevel() const;
  bool IsAtomicInlineLevel() const;
  bool MayHaveAspectRatio() const;

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

  // CSS defines certain cases to synthesize inline block baselines from box.
  // See comments in UseLogicalBottomMarginEdgeForInlineBlockBaseline().
  bool UseLogicalBottomMarginEdgeForInlineBlockBaseline() const;

  // Returns true if the custom layout node is in its loaded state (all script
  // for the web-developer defined layout is ready).
  bool IsCustomLayoutLoaded() const;

  // Layout an atomic inline; e.g., inline block.
  scoped_refptr<const NGLayoutResult> LayoutAtomicInline(
      const NGConstraintSpace& parent_constraint_space,
      const ComputedStyle& parent_style,
      FontBaseline,
      bool use_first_line_style);

  // Called if this is an out-of-flow block which needs to be
  // positioned with legacy layout.
  void UseLegacyOutOfFlowPositioning() const;

  // Save static position for legacy AbsPos layout.
  void SaveStaticOffsetForLegacy(const LogicalOffset&,
                                 const LayoutObject* offset_container);

  // Write back resolved margins to legacy.
  void StoreMargins(const NGConstraintSpace&, const NGBoxStrut& margins);

  static bool CanUseNewLayout(const LayoutBox&);
  bool CanUseNewLayout() const;

  String ToString() const;

 private:
  void PrepareForLayout();

  // Runs layout on the underlying LayoutObject and creates a fragment for the
  // resulting geometry.
  scoped_refptr<const NGLayoutResult> RunLegacyLayout(const NGConstraintSpace&);

  scoped_refptr<const NGLayoutResult> RunSimplifiedLayout(
      const NGLayoutAlgorithmParams&) const;

  // If this node is a LayoutNGMixin, the caller must pass the layout object for
  // this node cast to a LayoutBlockFlow as the first argument.
  void FinishLayout(LayoutBlockFlow*,
                    const NGConstraintSpace&,
                    const NGBreakToken*,
                    scoped_refptr<const NGLayoutResult>);

  // After we run the layout algorithm, this function copies back the geometry
  // data to the layout box.
  void CopyFragmentDataToLayoutBox(
      const NGConstraintSpace&,
      const NGLayoutResult&,
      const NGBlockBreakToken* previous_break_token);
  void CopyFragmentDataToLayoutBoxForInlineChildren(
      const NGPhysicalBoxFragment& container);
  void CopyFragmentDataToLayoutBoxForInlineChildren(
      const NGPhysicalContainerFragment& container,
      LayoutUnit initial_container_width,
      bool initial_container_is_flipped,
      PhysicalOffset offset = {});
  void PlaceChildrenInLayoutBox(const NGPhysicalBoxFragment&,
                                const PhysicalOffset& offset_from_start);
  void PlaceChildrenInFlowThread(const NGPhysicalBoxFragment&);
  void CopyChildFragmentPosition(
      const NGPhysicalFragment& fragment,
      const PhysicalOffset fragment_offset,
      const PhysicalOffset additional_offset = PhysicalOffset());

  void CopyBaselinesFromLegacyLayout(const NGConstraintSpace&,
                                     NGBoxFragmentBuilder*);
  LayoutUnit AtomicInlineBaselineFromLegacyLayout(const NGBaselineRequest&,
                                                  const NGConstraintSpace&);

  void UpdateShapeOutsideInfoIfNeeded(
      const NGLayoutResult&,
      LayoutUnit percentage_resolution_inline_size);
};

template <>
struct DowncastTraits<NGBlockNode> {
  static bool AllowFrom(const NGLayoutInputNode& node) {
    return node.IsBlock();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_NODE_H_
