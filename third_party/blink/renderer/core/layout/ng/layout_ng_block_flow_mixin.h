// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_FLOW_MIXIN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_FLOW_MIXIN_H_

#include <type_traits>

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"

namespace blink {

enum class NGBaselineAlgorithmType;
struct NGInlineNodeData;

// This mixin holds code shared between LayoutNG subclasses of LayoutBlockFlow.
//
// If you'd like to make a LayoutNGFoo class inheriting from
// LayoutNGBlockFlowMixin<LayoutBar>, you need to do:
//  * Add the following to the header for LayoutNGFoo.
//     extern template class CORE_EXTERN_TEMPLATE_EXPORT
//         LayoutNGBlockFlowMixin<LayoutBar>;
//     extern template class CORE_EXTERN_TEMPLATE_EXPORT
//         LayoutNGMixin<LayoutBar>;
//  * Add |#include "header for LayoutNGFoo"| to layout_ng_block_flow_mixin.cc
//    and layout_ng_mixin.cc.
//    It's the header for LayoutNGFoo, not for LayoutBar. The purpose is to
//    include the above |extern template| declarations.
//  * Add |template class CORE_TEMPLATE_EXPORT
//    LayoutNGBlockFlowMixin<LayoutBar>;| to layout_ng_block_flow_mixin.cc.
//  * Add |template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutBar>;| to
//    layout_ng_mixin.cc.
template <typename Base>
class LayoutNGBlockFlowMixin : public LayoutNGMixin<Base> {
 public:
  explicit LayoutNGBlockFlowMixin(ContainerNode*);
  ~LayoutNGBlockFlowMixin() override;

  NGInlineNodeData* TakeNGInlineNodeData() final;
  NGInlineNodeData* GetNGInlineNodeData() const final;
  void ResetNGInlineNodeData() final;
  void ClearNGInlineNodeData() final;
  bool HasNGInlineNodeData() const final;

  LayoutUnit FirstLineBoxBaseline() const final;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const final;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestPhase) override;

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  void Trace(Visitor*) const override;

 protected:
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

#if DCHECK_IS_ON()
  void AddLayoutOverflowFromChildren() final;
#endif

  void AddOutlineRects(Vector<PhysicalRect>&,
                       LayoutObject::OutlineInfo*,
                       const PhysicalOffset& additional_offset,
                       NGOutlineType) const final;

  void DirtyLinesFromChangedChild(LayoutObject* child,
                                  MarkingBehavior marking_behavior) final;

  // Intended to be called from UpdateLayout() for subclasses that want the same
  // behavior as LayoutNGBlockFlow.
  void UpdateNGBlockLayout();

  Member<NGInlineNodeData> ng_inline_node_data_;

  friend class NGBaseLayoutAlgorithmTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_FLOW_MIXIN_H_
