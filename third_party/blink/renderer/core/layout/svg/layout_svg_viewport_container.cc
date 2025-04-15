/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"

#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_info.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"

namespace blink {

LayoutSVGViewportContainer::LayoutSVGViewportContainer(SVGSVGElement* node)
    : LayoutSVGContainer(node) {}

SVGLayoutResult LayoutSVGViewportContainer::UpdateSVGLayout(
    const SVGLayoutInfo& layout_info) {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  SVGLayoutInfo child_layout_info = layout_info;
  child_layout_info.viewport_changed = SelfNeedsFullLayout();

  if (SelfNeedsFullLayout()) {
    const auto* svg = To<SVGSVGElement>(GetElement());
    SVGLengthContext length_context(svg);
    gfx::RectF old_viewport = viewport_;
    viewport_.SetRect(svg->x()->CurrentValue()->Value(length_context),
                      svg->y()->CurrentValue()->Value(length_context),
                      svg->width()->CurrentValue()->Value(length_context),
                      svg->height()->CurrentValue()->Value(length_context));
    if (old_viewport != viewport_) {
      // The transform depends on viewport values.
      SetNeedsTransformUpdate();
    }
  }

  return LayoutSVGContainer::UpdateSVGLayout(child_layout_info);
}

SVGTransformChange LayoutSVGViewportContainer::UpdateLocalTransform(
    const gfx::RectF& reference_box) {
  NOT_DESTROYED();
  SVGTransformChangeDetector change_detector(local_to_parent_transform_);

  local_to_parent_transform_ = ComputeViewboxTransform();

  if (RuntimeEnabledFeatures::SvgTransformOnNestedSvgElementEnabled()) {
    local_transform_ = TransformHelper::ComputeTransformIncludingMotion(
        *GetElement(), reference_box);

    // If both `transform` and `viewBox` are applied to an element two new
    // coordinate systems are established. `transform` establishes the first new
    // coordinate system for the element. `viewBox` establishes a second
    // coordinate system for all descendants of the element. The first
    // coordinate system is post-multiplied by the second coordinate system.
    //
    // https://svgwg.org/svg2-draft/coords.html#ViewBoxAttribute
    local_to_parent_transform_ = local_transform_ * local_to_parent_transform_;
  }

  return change_detector.ComputeChange(local_to_parent_transform_);
}

gfx::RectF LayoutSVGViewportContainer::ViewBoxRect() const {
  return To<SVGSVGElement>(*GetElement()).CurrentViewBoxRect();
}

bool LayoutSVGViewportContainer::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestPhase phase) {
  NOT_DESTROYED();
  // Respect the viewport clip which is in parent coordinates.
  if (SVGLayoutSupport::IsOverflowHidden(*this)) {
    TransformedHitTestLocation local_transformed_hit_location(
        hit_test_location, LocalSVGTransform());

    if (!local_transformed_hit_location ||
        !local_transformed_hit_location->Intersects(viewport_)) {
      return false;
    }
  }
  return LayoutSVGContainer::NodeAtPoint(result, hit_test_location,
                                         accumulated_offset, phase);
}

void LayoutSVGViewportContainer::IntersectChildren(
    HitTestResult& result,
    const HitTestLocation& location) const {
  Content().HitTest(result, location, HitTestPhase::kForeground);
}

AffineTransform LayoutSVGViewportContainer::ComputeViewboxTransform() const {
  NOT_DESTROYED();
  const auto* svg = To<SVGSVGElement>(GetElement());

  return AffineTransform::Translation(viewport_.x(), viewport_.y()) *
         svg->ViewBoxToViewTransform(viewport_.size());
}

void LayoutSVGViewportContainer::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGContainer::StyleDidChange(diff, old_style);
  const ComputedStyle& style = StyleRef();

  if (old_style && (SVGLayoutSupport::IsOverflowHidden(*old_style) !=
                    SVGLayoutSupport::IsOverflowHidden(style))) {
    // See NeedsOverflowClip() in PaintPropertyTreeBuilder for the reason.
    SetNeedsPaintPropertyUpdate();
  }

  // TODO: Inherit `LayoutSVGViewportContainer` from
  // `LayoutSVGTransformableContainer` so below bits of code can be shared.
  if (RuntimeEnabledFeatures::SvgTransformOnNestedSvgElementEnabled()) {
    TransformHelper::UpdateOffsetPath(*GetElement(), old_style);
    SetTransformUsesReferenceBox(TransformHelper::DependsOnReferenceBox(style));
  }
}

}  // namespace blink
