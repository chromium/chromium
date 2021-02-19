// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_FLOW_MIXIN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_FLOW_MIXIN_H_

#include <type_traits>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_progress.h"
#include "third_party/blink/renderer/core/layout/layout_ruby.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_run.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"

namespace blink {

enum class NGBaselineAlgorithmType;
struct NGInlineNodeData;

// This mixin holds code shared between LayoutNG subclasses of LayoutBlockFlow.
template <typename Base>
class LayoutNGBlockFlowMixin : public LayoutNGMixin<Base> {
 public:
  explicit LayoutNGBlockFlowMixin(Element* element);
  ~LayoutNGBlockFlowMixin() override;

  NGInlineNodeData* TakeNGInlineNodeData() final;
  NGInlineNodeData* GetNGInlineNodeData() const final;
  void ResetNGInlineNodeData() final;
  void ClearNGInlineNodeData() final;
  bool HasNGInlineNodeData() const final { return ng_inline_node_data_.get(); }

  LayoutUnit FirstLineBoxBaseline() const final;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const final;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) override;

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  using LayoutNGMixin<Base>::CurrentFragment;

 protected:
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void AddLayoutOverflowFromChildren() final;

  void AddOutlineRects(Vector<PhysicalRect>&,
                       const PhysicalOffset& additional_offset,
                       NGOutlineType) const final;

  void DirtyLinesFromChangedChild(LayoutObject* child,
                                  MarkingBehavior marking_behavior) final;

  // Intended to be called from UpdateLayout() for subclasses that want the same
  // behavior as LayoutNGBlockFlow.
  void UpdateNGBlockLayout();

  std::unique_ptr<NGInlineNodeData> ng_inline_node_data_;

  friend class NGBaseLayoutAlgorithmTest;

 private:
  void AddScrollingOverflowFromChildren();
  void UpdateMargins();
};

// If you edit these export templates, also update templates in
// layout_ng_mixin.h.
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutBlockFlow>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutProgress>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutRubyAsBlock>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutRubyBase>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutRubyRun>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutRubyText>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutTableCaption>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutTableCell>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_FLOW_MIXIN_H_
