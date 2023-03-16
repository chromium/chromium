// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_FOREIGN_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_FOREIGN_OBJECT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_block.h"

namespace blink {

extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutSVGBlock>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutSVGBlock>;

// The LayoutNG representation of SVG <foreignObject>.
class LayoutNGSVGForeignObject final
    : public LayoutNGBlockFlowMixin<LayoutSVGBlock> {
 public:
  explicit LayoutNGSVGForeignObject(Element* element);

  bool IsObjectBoundingBoxValid() const;

  // A method to call when recursively hit testing from an SVG parent.
  // Since LayoutSVGRoot has a PaintLayer always, this will cause a
  // trampoline through PaintLayer::HitTest and back to a call to NodeAtPoint
  // on this object. This is why there are two methods.
  bool NodeAtPointFromSVG(HitTestResult& result,
                          const HitTestLocation& hit_test_location,
                          const PhysicalOffset& accumulated_offset,
                          HitTestPhase phase);

 private:
  // LayoutObject override:
  const char* GetName() const override;
  bool IsOfType(LayoutObjectType type) const override;
  bool IsChildAllowed(LayoutObject* child,
                      const ComputedStyle& style) const override;
  gfx::RectF ObjectBoundingBox() const override;
  gfx::RectF StrokeBoundingBox() const override;
  gfx::RectF VisualRectInLocalSVGCoordinates() const override;
  AffineTransform LocalToSVGParentTransform() const override;

  // LayoutBox override:
  LayoutPoint Location() const override;
  PaintLayerType LayerTypeRequired() const override;
  bool CreatesNewFormattingContext() const override;

  // LayoutBlock override:
  void UpdateBlockLayout(bool relayout_children) override;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  // The resolved viewport in the regular SVG coordinate space (after any
  // 'transform' has been applied but without zoom-adjustment).
  gfx::RectF viewport_;

  // Override of LayoutBox::frame_rect_.location_.
  // A physical fragment for <foreignObject> doesn't have the owner NGLink.
  LayoutPoint overridden_location_;
};

template <>
struct DowncastTraits<LayoutNGSVGForeignObject> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsNGSVGForeignObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_FOREIGN_OBJECT_H_
