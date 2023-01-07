/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"

#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/svg_foreign_object_painter.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"

namespace blink {

LayoutSVGForeignObject::LayoutSVGForeignObject(Element* element)
    : LayoutSVGBlock(element) {
  DCHECK(IsA<SVGForeignObjectElement>(element));
}

LayoutSVGForeignObject::~LayoutSVGForeignObject() = default;

bool LayoutSVGForeignObject::IsChildAllowed(LayoutObject* child,
                                            const ComputedStyle& style) const {
  NOT_DESTROYED();
  // Disallow arbitary SVG content. Only allow proper <svg xmlns="svgNS">
  // subdocuments.
  return !child->IsSVGChild();
}

void LayoutSVGForeignObject::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  SVGForeignObjectPainter(*this).Paint(paint_info);
}

void LayoutSVGForeignObject::UpdateLogicalWidth() {
  NOT_DESTROYED();
  const ComputedStyle& style = StyleRef();
  float logical_width =
      style.IsHorizontalWritingMode() ? viewport_.width() : viewport_.height();
  logical_width *= style.EffectiveZoom();
  SetLogicalWidth(LayoutUnit(logical_width));
}

void LayoutSVGForeignObject::ComputeLogicalHeight(
    LayoutUnit,
    LayoutUnit logical_top,
    LogicalExtentComputedValues& computed_values) const {
  NOT_DESTROYED();
  const ComputedStyle& style = StyleRef();
  float logical_height =
      style.IsHorizontalWritingMode() ? viewport_.height() : viewport_.width();
  logical_height *= style.EffectiveZoom();
  computed_values.extent_ = LayoutUnit(logical_height);
  computed_values.position_ = logical_top;
}

AffineTransform LayoutSVGForeignObject::LocalToSVGParentTransform() const {
  NOT_DESTROYED();
  // Include a zoom inverse in the local-to-parent transform since descendants
  // of the <foreignObject> will have regular zoom applied, and thus need to
  // have that removed when moving into the <fO> ancestors chain (the SVG root
  // will then reapply the zoom again if that boundary is crossed).
  AffineTransform transform = local_transform_;
  transform.Scale(1 / StyleRef().EffectiveZoom());
  return transform;
}

void LayoutSVGForeignObject::UpdateLayout() {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  auto* foreign = To<SVGForeignObjectElement>(GetElement());

  // Update our transform before layout, in case any of our descendants rely on
  // the transform being somewhat accurate.  The |needs_transform_update_| flag
  // will be cleared after layout has been performed.
  // TODO(fs): Remove this. AFAICS in all cases where descendants compute some
  // form of CTM, they stop at their nearest ancestor LayoutSVGRoot, and thus
  // will not care about (reach) this value.
  if (needs_transform_update_) {
    local_transform_ =
        foreign->CalculateTransform(SVGElement::kIncludeMotionTransform);
  }

  LayoutRect old_frame_rect = FrameRect();

  // Resolve the viewport in the local coordinate space - this does not include
  // zoom.
  SVGLengthContext length_context(foreign);
  const ComputedStyle& style = StyleRef();
  gfx::Vector2dF origin =
      length_context.ResolveLengthPair(style.X(), style.Y(), style);
  gfx::Vector2dF size =
      length_context.ResolveLengthPair(style.Width(), style.Height(), style);
  // SetRect() will clamp negative width/height to zero.
  viewport_.SetRect(origin.x(), origin.y(), size.x(), size.y());

  // Use the zoomed version of the viewport as the location, because we will
  // interpose a transform that "unzooms" the effective zoom to let the children
  // of the foreign object exist with their specified zoom.
  gfx::PointF zoomed_location =
      gfx::ScalePoint(viewport_.origin(), style.EffectiveZoom());

  // Set box origin to the foreignObject x/y translation, so positioned objects
  // in XHTML content get correct positions. A regular LayoutBoxModelObject
  // would pull this information from ComputedStyle - in SVG those properties
  // are ignored for non <svg> elements, so we mimic what happens when
  // specifying them through CSS.
  SetLocation(LayoutPoint(zoomed_location));

  LayoutBlock::UpdateLayout();
  DCHECK(!NeedsLayout());
  const bool bounds_changed = old_frame_rect != FrameRect();

  // Invalidate all resources of this client if our reference box changed.
  if (EverHadLayout() && bounds_changed)
    SVGResourceInvalidator(*this).InvalidateEffects();

  bool update_parent_boundaries = bounds_changed;
  if (UpdateTransformAfterLayout(bounds_changed))
    update_parent_boundaries = true;

  // Notify ancestor about our bounds changing.
  if (update_parent_boundaries)
    LayoutSVGBlock::SetNeedsBoundariesUpdate();

  DCHECK(!needs_transform_update_);
}

bool LayoutSVGForeignObject::NodeAtPointFromSVG(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestPhase) {
  NOT_DESTROYED();
  DCHECK_EQ(accumulated_offset, PhysicalOffset());
  TransformedHitTestLocation local_location(hit_test_location,
                                            LocalToSVGParentTransform());
  if (!local_location)
    return false;

  // |local_location| already includes the offset of the <foreignObject>
  // element, but PaintLayer::HitTestLayer assumes it has not been.
  HitTestLocation local_without_offset(*local_location, -PhysicalLocation());
  HitTestResult layer_result(result.GetHitTestRequest(), local_without_offset);
  bool retval = Layer()->HitTest(local_without_offset, layer_result,
                                 PhysicalRect(PhysicalRect::InfiniteIntRect()));

  // Preserve the "point in inner node frame" from the original request,
  // since |layer_result| is a hit test rooted at the <foreignObject> element,
  // not the frame, due to the constructor above using
  // |point_in_foreign_object| as its "point in inner node frame".
  // TODO(chrishtr): refactor the PaintLayer and HitTestResults code around
  // this, to better support hit tests that don't start at frame boundaries.
  PhysicalOffset original_point_in_inner_node_frame =
      result.PointInInnerNodeFrame();
  if (result.GetHitTestRequest().ListBased())
    result.Append(layer_result);
  else
    result = layer_result;
  result.SetPointInInnerNodeFrame(original_point_in_inner_node_frame);
  return retval;
}

bool LayoutSVGForeignObject::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestPhase phase) {
  NOT_DESTROYED();
  // Skip LayoutSVGBlock's override.
  return LayoutBlockFlow::NodeAtPoint(result, hit_test_location,
                                      accumulated_offset, phase);
}

PaintLayerType LayoutSVGForeignObject::LayerTypeRequired() const {
  NOT_DESTROYED();
  // Skip LayoutSVGBlock's override.
  return LayoutBlockFlow::LayerTypeRequired();
}

}  // namespace blink
