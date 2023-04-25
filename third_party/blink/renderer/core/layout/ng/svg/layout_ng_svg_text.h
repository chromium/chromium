// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_TEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_block.h"

namespace blink {

extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutSVGBlock>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutSVGBlock>;

// The LayoutNG representation of SVG <text>.
class LayoutNGSVGText final : public LayoutNGBlockFlowMixin<LayoutSVGBlock> {
 public:
  explicit LayoutNGSVGText(Element* element);

  void SubtreeStructureChanged(LayoutInvalidationReasonForTracing);
  // This is called whenever a text layout attribute on the <text> or a
  // descendant <tspan> is changed.
  void SetNeedsPositioningValuesUpdate();
  void SetNeedsTextMetricsUpdate();
  bool NeedsTextMetricsUpdate() const;

  bool IsObjectBoundingBoxValid() const;

  // These two functions return a LayoutNGSVGText or nullptr.
  static LayoutNGSVGText* LocateLayoutSVGTextAncestor(LayoutObject*);
  static const LayoutNGSVGText* LocateLayoutSVGTextAncestor(
      const LayoutObject*);

  static void NotifySubtreeStructureChanged(LayoutObject*,
                                            LayoutInvalidationReasonForTracing);

 private:
  // LayoutObject override:
  const char* GetName() const override;
  bool IsOfType(LayoutObjectType type) const override;
  bool IsChildAllowed(LayoutObject* child, const ComputedStyle&) const override;
  void AddChild(LayoutObject* child, LayoutObject* before_child) override;
  void RemoveChild(LayoutObject* child) override;
  void InsertedIntoTree() override;
  void WillBeRemovedFromTree() override;
  gfx::RectF ObjectBoundingBox() const override;
  gfx::RectF StrokeBoundingBox() const override;
  gfx::RectF VisualRectInLocalSVGCoordinates() const override;
  void AbsoluteQuads(Vector<gfx::QuadF>& quads,
                     MapCoordinatesFlags mode) const override;
  gfx::RectF LocalBoundingBoxRectForAccessibility() const override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void WillBeDestroyed() override;
  bool NodeAtPoint(HitTestResult& result,
                   const HitTestLocation& hit_test_location,
                   const PhysicalOffset& accumulated_offset,
                   HitTestPhase phase) override;
  PositionWithAffinity PositionForPoint(
      const PhysicalOffset& point_in_contents) const override;

  // LayoutBox override:
  bool CreatesNewFormattingContext() const override;
  void UpdateFromStyle() override;

  // LayoutBlock override:
  void Paint(const PaintInfo&) const override;
  void UpdateBlockLayout() override;

  void UpdateFont();
  void UpdateTransformAffectsVectorEffect();

  // bounding_box_* are mutable for on-demand computation in a const method.
  mutable gfx::RectF bounding_box_;
  mutable bool needs_update_bounding_box_ : 1;

  bool needs_text_metrics_update_ : 1;
};

template <>
struct DowncastTraits<LayoutNGSVGText> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsNGSVGText();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_TEXT_H_
