// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGMixin_h
#define LayoutNGMixin_h

#include <type_traits>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"

namespace blink {

enum class NGBaselineAlgorithmType;
class NGBreakToken;
class NGConstraintSpace;
class NGLayoutResult;
class NGPaintFragment;
class NGPhysicalFragment;
struct NGBaseline;
struct NGInlineNodeData;
struct NGPhysicalOffset;

// This mixin holds code shared between LayoutNG subclasses of
// LayoutBlockFlow.

template <typename Base>
class LayoutNGMixin : public Base {
 public:
  explicit LayoutNGMixin(Element* element);
  ~LayoutNGMixin() override;

  bool IsLayoutNGObject() const final { return true; }

  NGInlineNodeData* TakeNGInlineNodeData() final;
  NGInlineNodeData* GetNGInlineNodeData() const final;
  void ResetNGInlineNodeData() final;
  bool HasNGInlineNodeData() const final { return ng_inline_node_data_.get(); }

  LayoutUnit FirstLineBoxBaseline() const final;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const final;

  void InvalidateDisplayItemClients(PaintInvalidationReason) const final;

  void Paint(const PaintInfo&) const final;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& location_in_container,
                   const LayoutPoint& accumulated_offset,
                   HitTestAction) final;

  PositionWithAffinity PositionForPoint(const LayoutPoint&) const final;

  // Returns the last layout result for this block flow with the given
  // constraint space and break token, or null if it is not up-to-date or
  // otherwise unavailable.
  scoped_refptr<NGLayoutResult> CachedLayoutResult(
      const NGConstraintSpace&,
      const NGBreakToken*) const final;

  void SetCachedLayoutResult(const NGConstraintSpace&,
                             const NGBreakToken*,
                             const NGLayoutResult&) final;
  void ClearCachedLayoutResult() final;

  // For testing only.
  scoped_refptr<const NGLayoutResult> CachedLayoutResultForTesting() final;

  NGPaintFragment* PaintFragment() const final { return paint_fragment_.get(); }
  void SetPaintFragment(const NGBreakToken*,
                        scoped_refptr<const NGPhysicalFragment>,
                        NGPhysicalOffset) final;
  void UpdatePaintFragmentFromCachedLayoutResult(
      const NGBreakToken*,
      scoped_refptr<const NGPhysicalFragment>,
      NGPhysicalOffset) final;

 protected:
  bool IsOfType(LayoutObject::LayoutObjectType) const override;

  void ComputeVisualOverflow(const LayoutRect&, bool recompute_floats) final;

  void AddVisualOverflowFromChildren();
  void AddLayoutOverflowFromChildren() final;

 private:
  void AddScrollingOverflowFromChildren();
  void SetPaintFragment(scoped_refptr<const NGPhysicalFragment> fragment,
                        NGPhysicalOffset offset,
                        scoped_refptr<NGPaintFragment>* current);

 protected:
  void AddOutlineRects(Vector<LayoutRect>&,
                       const LayoutPoint& additional_offset,
                       NGOutlineType) const final;

  const NGPhysicalBoxFragment* CurrentFragment() const final;

  const NGBaseline* FragmentBaseline(NGBaselineAlgorithmType) const;

  void DirtyLinesFromChangedChild(LayoutObject* child,
                                  MarkingBehavior marking_behavior) final;

  std::unique_ptr<NGInlineNodeData> ng_inline_node_data_;

  scoped_refptr<const NGLayoutResult> cached_result_;
  scoped_refptr<NGPaintFragment> paint_fragment_;

  friend class NGBaseLayoutAlgorithmTest;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutBlockFlow>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutTableCaption>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutTableCell>;

}  // namespace blink

#endif  // LayoutNGMixin_h
