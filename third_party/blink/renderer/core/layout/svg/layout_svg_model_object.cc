/*
 * Copyright (c) 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_model_object.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"

namespace blink {

LayoutSVGModelObject::LayoutSVGModelObject(SVGElement* node)
    : LayoutObject(node) {}

bool LayoutSVGModelObject::IsChildAllowed(LayoutObject* child,
                                          const ComputedStyle&) const {
  return child->IsSVG() && !(child->IsSVGInline() || child->IsSVGInlineText());
}

void LayoutSVGModelObject::MapLocalToAncestor(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    MapCoordinatesFlags flags) const {
  SVGLayoutSupport::MapLocalToAncestor(this, ancestor, transform_state, flags);
}

PhysicalRect LayoutSVGModelObject::VisualRectInDocument(
    VisualRectFlags flags) const {
  return SVGLayoutSupport::VisualRectInAncestorSpace(*this, *View(), flags);
}

void LayoutSVGModelObject::MapAncestorToLocal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    MapCoordinatesFlags flags) const {
  SVGLayoutSupport::MapAncestorToLocal(*this, ancestor, transform_state, flags);
}

const LayoutObject* LayoutSVGModelObject::PushMappingToContainer(
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) const {
  return SVGLayoutSupport::PushMappingToContainer(this, ancestor_to_stop_at,
                                                  geometry_map);
}

void LayoutSVGModelObject::AbsoluteQuads(Vector<FloatQuad>& quads,
                                         MapCoordinatesFlags mode) const {
  quads.push_back(LocalToAbsoluteQuad(StrokeBoundingBox(), mode));
}

// This method is called from inside PaintOutline(), and since we call
// PaintOutline() while transformed to our coord system, return local coords.
void LayoutSVGModelObject::AddOutlineRects(Vector<PhysicalRect>& rects,
                                           const PhysicalOffset&,
                                           NGOutlineType) const {
  rects.push_back(
      PhysicalRect::EnclosingRect(VisualRectInLocalSVGCoordinates()));
}

FloatRect LayoutSVGModelObject::LocalBoundingBoxRectForAccessibility() const {
  return StrokeBoundingBox();
}

void LayoutSVGModelObject::WillBeDestroyed() {
  SVGResourcesCache::ClientDestroyed(*this);
  SVGResources::ClearClipPathFilterMask(*GetElement(), Style());
  LayoutObject::WillBeDestroyed();
}

AffineTransform LayoutSVGModelObject::CalculateLocalTransform() const {
  auto* element = GetElement();
  if (element->HasTransform(SVGElement::kIncludeMotionTransform))
    return element->CalculateTransform(SVGElement::kIncludeMotionTransform);
  return AffineTransform();
}

bool LayoutSVGModelObject::CheckForImplicitTransformChange(
    bool bbox_changed) const {
  // If the transform is relative to the reference box, check relevant
  // conditions to see if we need to recompute the transform.
  switch (StyleRef().TransformBox()) {
    case ETransformBox::kViewBox:
      return SVGLayoutSupport::LayoutSizeOfNearestViewportChanged(this);
    case ETransformBox::kFillBox:
      return bbox_changed;
  }
  NOTREACHED();
  return false;
}

void LayoutSVGModelObject::StyleDidChange(StyleDifference diff,
                                          const ComputedStyle* old_style) {
  // Since layout depends on the bounds of the filter, we need to force layout
  // when the filter changes. We also need to make sure paint will be
  // performed, since if the filter changed we will not have cached result from
  // before and thus will not flag paint in ClientLayoutChanged.
  if (diff.FilterChanged()) {
    SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kStyleChange);
  }

  if (diff.NeedsFullLayout()) {
    SetNeedsBoundariesUpdate();
    if (diff.TransformChanged())
      SetNeedsTransformUpdate();
  }

  if (IsBlendingAllowed()) {
    bool has_blend_mode_changed =
        (old_style && old_style->HasBlendMode()) == !StyleRef().HasBlendMode();
    if (Parent() && has_blend_mode_changed) {
      Parent()->DescendantIsolationRequirementsChanged(
          StyleRef().HasBlendMode() ? kDescendantIsolationRequired
                                    : kDescendantIsolationNeedsUpdate);
    }

    if (has_blend_mode_changed)
      SetNeedsPaintPropertyUpdate();
  }

  LayoutObject::StyleDidChange(diff, old_style);
  SVGResources::UpdateClipPathFilterMask(*GetElement(), old_style, StyleRef());
  SVGResourcesCache::ClientStyleChanged(*this, diff, StyleRef());
}

}  // namespace blink
