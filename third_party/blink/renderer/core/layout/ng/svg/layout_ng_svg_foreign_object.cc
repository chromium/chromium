// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_foreign_object.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"

namespace blink {

LayoutNGSVGForeignObject::LayoutNGSVGForeignObject(Element* element)
    : LayoutNGBlockFlowMixin<LayoutSVGBlock>(element) {
  DCHECK(IsA<SVGForeignObjectElement>(element));
}

const char* LayoutNGSVGForeignObject::GetName() const {
  NOT_DESTROYED();
  return "LayoutNGSVGForeignObject";
}

bool LayoutNGSVGForeignObject::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectNGSVGForeignObject ||
         LayoutNGBlockFlowMixin<LayoutSVGBlock>::IsOfType(type);
}

bool LayoutNGSVGForeignObject::IsChildAllowed(
    LayoutObject* child,
    const ComputedStyle& style) const {
  NOT_DESTROYED();
  // Disallow arbitrary SVG content. Only allow proper <svg xmlns="svgNS">
  // subdocuments.
  return !child->IsSVGChild();
}

bool LayoutNGSVGForeignObject::IsObjectBoundingBoxValid() const {
  NOT_DESTROYED();
  return !viewport_.IsEmpty();
}

gfx::RectF LayoutNGSVGForeignObject::ObjectBoundingBox() const {
  NOT_DESTROYED();
  return viewport_;
}

gfx::RectF LayoutNGSVGForeignObject::StrokeBoundingBox() const {
  NOT_DESTROYED();
  return VisualRectInLocalSVGCoordinates();
}

gfx::RectF LayoutNGSVGForeignObject::VisualRectInLocalSVGCoordinates() const {
  NOT_DESTROYED();
  return gfx::RectF(FrameRect());
}

AffineTransform LayoutNGSVGForeignObject::LocalToSVGParentTransform() const {
  NOT_DESTROYED();
  // Include a zoom inverse in the local-to-parent transform since descendants
  // of the <foreignObject> will have regular zoom applied, and thus need to
  // have that removed when moving into the <fO> ancestors chain (the SVG root
  // will then reapply the zoom again if that boundary is crossed).
  AffineTransform transform = local_transform_;
  transform.Scale(1 / StyleRef().EffectiveZoom());
  return transform;
}

LayoutPoint LayoutNGSVGForeignObject::Location() const {
  NOT_DESTROYED();
  return overridden_location_;
}

PaintLayerType LayoutNGSVGForeignObject::LayerTypeRequired() const {
  NOT_DESTROYED();
  // Skip LayoutSVGBlock's override.
  return LayoutBlockFlow::LayerTypeRequired();
}

bool LayoutNGSVGForeignObject::CreatesNewFormattingContext() const {
  NOT_DESTROYED();
  // This is the root of a foreign object. Don't let anything inside it escape
  // to our ancestors.
  return true;
}

void LayoutNGSVGForeignObject::UpdateBlockLayout(bool relayout_children) {
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
  gfx::Vector2dF size = length_context.ResolveLengthPair(
      style.UsedWidth(), style.UsedHeight(), style);
  // SetRect() will clamp negative width/height to zero.
  viewport_.SetRect(origin.x(), origin.y(), size.x(), size.y());

  // A generated physical fragment should have the size for viewport_.
  // This is necessary for external/wpt/inert/inert-on-non-html.html.
  // See FullyClipsContents() in fully_clipped_state_stack.cc.
  const float zoom = style.EffectiveZoom();
  LogicalSize zoomed_size = PhysicalSize(LayoutUnit(viewport_.width() * zoom),
                                         LayoutUnit(viewport_.height() * zoom))
                                .ConvertToLogical(style.GetWritingMode());

  if (!RuntimeEnabledFeatures::LayoutNewSVGForeignObjectEntryEnabled()) {
    SetOverrideLogicalWidth(zoomed_size.inline_size);
    SetOverrideLogicalHeight(zoomed_size.block_size);
  }

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
  overridden_location_ = LayoutPoint(zoomed_location);

  if (RuntimeEnabledFeatures::LayoutNewSVGForeignObjectEntryEnabled()) {
    NGConstraintSpaceBuilder builder(
        style.GetWritingMode(), style.GetWritingDirection(),
        /* is_new_fc */ true, /* adjust_inline_size_if_needed */ false);
    builder.SetAvailableSize(zoomed_size);
    builder.SetIsFixedInlineSize(true);
    builder.SetIsFixedBlockSize(true);
    NGBlockNode(this).Layout(builder.ToConstraintSpace());
  } else {
    UpdateNGBlockLayout();
  }

  DCHECK(!NeedsLayout() || ChildLayoutBlockedByDisplayLock());
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

void LayoutNGSVGForeignObject::StyleDidChange(StyleDifference diff,
                                              const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutNGBlockFlowMixin<LayoutSVGBlock>::StyleDidChange(diff, old_style);

  float old_zoom = old_style ? old_style->EffectiveZoom()
                             : ComputedStyleInitialValues::InitialZoom();
  if (StyleRef().EffectiveZoom() != old_zoom) {
    // `LocalToSVGParentTransform` has a dependency on zoom which is used for
    // the transform paint property.
    SetNeedsPaintPropertyUpdate();
  }
}

bool LayoutNGSVGForeignObject::NodeAtPointFromSVG(
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

}  // namespace blink
