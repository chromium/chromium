// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_MIXIN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_MIXIN_H_

#include <type_traits>

#include "third_party/blink/renderer/core/layout/layout_block.h"

namespace blink {

// This mixin holds code shared between LayoutNG subclasses of
// LayoutBlock.
//
// If you'd like to make a LayoutNGFoo class inheriting from
// LayoutNGMixin<LayoutBar>, you need to do:
//  * Add the following to the header for LayoutNGFoo.
//     extern template class CORE_EXTERN_TEMPLATE_EXPORT
//         LayoutNGMixin<LayoutBar>;
//  * Add |#include "header for LayoutNGFoo"| to layout_ng_mixin.cc.
//    It's the header for LayoutNGFoo, not for LayoutBar. The purpose is to
//    include the above |extern template| declaration.
//  * Add |template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutBar>;| to
//    layout_ng_mixin.cc.
template <typename Base>
class LayoutNGMixin : public Base {
 public:
  explicit LayoutNGMixin(ContainerNode*);
  ~LayoutNGMixin() override;

  void Paint(const PaintInfo&) const override;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestPhase) override;
  RecalcLayoutOverflowResult RecalcLayoutOverflow() override;
  RecalcLayoutOverflowResult RecalcChildLayoutOverflow() override;
  void RecalcVisualOverflow() override;

  bool IsLayoutNGObject() const final;

 protected:
  MinMaxSizes ComputeIntrinsicLogicalWidths() const override;
  NGConstraintSpace ConstraintSpaceForMinMaxSizes() const;

  void UpdateOutOfFlowBlockLayout();
  const NGLayoutResult* UpdateInFlowBlockLayout();
  void UpdateMargins();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_MIXIN_H_
