// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_MIXIN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_MIXIN_H_

#include <type_traits>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_progress.h"
#include "third_party/blink/renderer/core/layout/layout_ruby.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_run.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_text.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"

namespace blink {

// This mixin holds code shared between LayoutNG subclasses of
// LayoutBlock.
template <typename Base>
class LayoutNGMixin : public Base {
 public:
  explicit LayoutNGMixin(Element* element);
  ~LayoutNGMixin() override;

  void Paint(const PaintInfo&) const override;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) override;
  RecalcLayoutOverflowResult RecalcLayoutOverflow() override;

  bool IsLayoutNGObject() const final { return true; }

  const NGPhysicalBoxFragment* CurrentFragment() const final;

 protected:
  bool IsOfType(LayoutObject::LayoutObjectType) const override;

  MinMaxSizes ComputeIntrinsicLogicalWidths() const override;
  NGConstraintSpace ConstraintSpaceForMinMaxSizes() const;

  void UpdateOutOfFlowBlockLayout();
  scoped_refptr<const NGLayoutResult> UpdateInFlowBlockLayout();
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutBlock>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutBlockFlow>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutProgress>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutRubyAsBlock>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutRubyBase>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutRubyRun>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutRubyText>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutTableCaption>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutTableCell>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_MIXIN_H_
