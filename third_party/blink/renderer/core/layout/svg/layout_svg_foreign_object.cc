// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"

#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_info.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"

namespace blink {

LayoutSVGForeignObject::LayoutSVGForeignObject(Element* element)
    : LayoutSVGBlock(element) {
  DCHECK(IsA<SVGForeignObjectElement>(element));
}

const char* LayoutSVGForeignObject::GetName() const {
  NOT_DESTROYED();
  return "LayoutSVGForeignObject";
}

bool LayoutSVGForeignObject::IsChildAllowed(LayoutObject* child,
                                            const ComputedStyle& style) const {
  NOT_DESTROYED();
  // Disallow arbitrary SVG content. Only allow proper <svg xmlns="svgNS">
  // subdocuments.
  return !child->IsSVGChild();
}

bool LayoutSVGForeignObject::IsObjectBoundingBoxValid() const {
  NOT_DESTROYED();
  return !viewport_.IsEmpty();
}

gfx::RectF LayoutSVGForeignObject::ObjectBoundingBox() const {
  NOT_DESTROYED();
  return viewport_;
}

gfx::RectF LayoutSVGForeignObject::StrokeBoundingBox() const {
  NOT_DESTROYED();
  return viewport_;
}

gfx::RectF LayoutSVGForeignObject::DecoratedBoundingBox() const {
  NOT_DESTROYED();
  return VisualRectInLocalSVGCoordinates();
}

gfx::RectF LayoutSVGForeignObject::VisualRectInLocalSVGCoordinates() const {
  NOT_DESTROYED();
  PhysicalOffset offset = PhysicalLocation();
  PhysicalSize size = Size();
  return gfx::RectF(offset.left, offset.top, size.width, size.height);
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

LayoutPoint LayoutSVGForeignObject::LocationInternal() const {
  NOT_DESTROYED();
  return overridden_location_;
}

PaintLayerType LayoutSVGForeignObject::LayerTypeRequired() const {
  NOT_DESTROYED();
  // Skip LayoutSVGBlock's override.
  return LayoutBlockFlow::LayerTypeRequired();
}

bool LayoutSVGForeignObject::CreatesNewFormattingContext() const {
  NOT_DESTROYED();
  // This is the root of a foreign object. Don't let anything inside it escape
  // to our ancestors.
  return true;
}

SVGLayoutResult LayoutSVGForeignObject::UpdateSVGLayout(
    const SVGLayoutInfo& layout_info) {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  // Update our transform before layout, in case any of our descendants rely on
  // the transform being somewhat accurate.  The |needs_transform_update_| flag
  // will be cleared after layout has been performed.
  // TODO(fs): Remove this. AFAICS in all cases where descendants compute some
  // form of CTM, they stop at their nearest ancestor LayoutSVGRoot, and thus
  // will not care about (reach) this value.
  UpdateTransformBeforeLayout();

  const PhysicalRect old_frame_rect(PhysicalLocation(), Size());

  // Resolve the viewport in the local coordinate space - this does not include
  // zoom.
  const SVGViewportResolver viewport_resolver(*this);
  const ComputedStyle& style = StyleRef();
  viewport_.set_origin(
      PointForLengthPair(style.X(), style.Y(), viewport_resolver, style));
  gfx::Vector2dF size = VectorForLengthPair(style.Width(), style.Height(),
                                            viewport_resolver, style);
  // gfx::SizeF() will clamp negative width/height to zero.
  viewport_.set_size(gfx::SizeF(size.x(), size.y()));

  // A generated physical fragment should have the size for viewport_.
  // This is necessary for external/wpt/inert/inert-on-non-html.html.
  // See FullyClipsContents() in fully_clipped_state_stack.cc.
  const float zoom = style.EffectiveZoom();
  LogicalSize zoomed_size = PhysicalSize(LayoutUnit(viewport_.width() * zoom),
                                         LayoutUnit(viewport_.height() * zoom))
                                .ConvertToLogical(style.GetWritingMode());

  // Use the zoomed version of the viewport as the location, because we will
  // interpose a transform that "unzooms" the effective zoom to let the children
  // of the foreign object exist with their specified zoom.
  gfx::PointF zoomed_location = gfx::ScalePoint(viewport_.origin(), zoom);

  // Set box origin to the foreignObject x/y translation, so positioned objects
  // in XHTML content get correct positions. A regular LayoutBoxModelObject
  // would pull this information from ComputedStyle - in SVG those properties
  // are ignored for non <svg> elements, so we mimic what happens when
  // specifying them through CSS.
  overridden_location_ = LayoutPoint(zoomed_location);

  ConstraintSpaceBuilder builder(
      style.GetWritingMode(), style.GetWritingDirection(),
      /* is_new_fc */ true, /* adjust_inline_size_if_needed */ false);
  builder.SetAvailableSize(zoomed_size);
  builder.SetIsFixedInlineSize(true);
  builder.SetIsFixedBlockSize(true);
  const auto* content_result =
      BlockNode(this).Layout(builder.ToConstraintSpace());

  // Any propagated sticky-descendants may have invalid sticky-constraints.
  // Clear them now.
  if (const auto* sticky_descendants =
          content_result->GetPhysicalFragment().PropagatedStickyDescendants()) {
    for (const auto& sticky_descendant : *sticky_descendants) {
      sticky_descendant->SetStickyConstraints(nullptr);
    }
  }

  DCHECK(!NeedsLayout() || ChildLayoutBlockedByDisplayLock());

  const PhysicalRect frame_rect(PhysicalLocation(), Size());
  const bool bounds_changed = old_frame_rect != frame_rect;

  SVGLayoutResult result;
  if (bounds_changed) {
    result.bounds_changed = true;
  }
  if (UpdateAfterSVGLayout(layout_info, bounds_changed)) {
    result.bounds_changed = true;
  }

  if (result.bounds_changed) {
    DeprecatedInvalidateIntersectionObserverCachedRects();
  }

  DCHECK(!needs_transform_update_);
  return result;
}

bool LayoutSVGForeignObject::UpdateAfterSVGLayout(
    const SVGLayoutInfo& layout_info,
    bool bounds_changed) {
  // Invalidate all resources of this client if our reference box changed.
  if (EverHadLayout() && bounds_changed) {
    SVGResourceInvalidator(*this).InvalidateEffects();
  }
  return UpdateTransformAfterLayout(layout_info, bounds_changed);
}

void LayoutSVGForeignObject::StyleDidChange(StyleDifference diff,
                                            const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGBlock::StyleDidChange(diff, old_style);

  float old_zoom = old_style ? old_style->EffectiveZoom()
                             : ComputedStyleInitialValues::InitialZoom();
  if (StyleRef().EffectiveZoom() != old_zoom) {
    // `LocalToSVGParentTransform` has a dependency on zoom which is used for
    // the transform paint property.
    SetNeedsPaintPropertyUpdate();
  }
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
  if (!local_location) {
    return false;
  }

  // |local_location| already includes the offset of the <foreignObject>
  // element, but PaintLayer::HitTestLayer assumes it has not been.
  HitTestLocation local_without_offset(*local_location, -PhysicalLocation());
  HitTestResult layer_result(result.GetHitTestRequest(), local_without_offset);
  bool retval = Layer()->HitTest(local_without_offset, layer_result,
                                 PhysicalRect(InfiniteIntRect()));

  // Preserve the "point in inner node frame" from the original request,
  // since |layer_result| is a hit test rooted at the <foreignObject> element,
  // not the frame, due to the constructor above using
  // |point_in_foreign_object| as its "point in inner node frame".
  // TODO(chrishtr): refactor the PaintLayer and HitTestResults code around
  // this, to better support hit tests that don't start at frame boundaries.
  PhysicalOffset original_point_in_inner_node_frame =
      result.PointInInnerNodeFrame();
  if (result.GetHitTestRequest().ListBased()) {
    result.Append(layer_result);
  } else {
    result = layer_result;
  }
  result.SetPointInInnerNodeFrame(original_point_in_inner_node_frame);
  return retval;
}

}  // namespace blink
