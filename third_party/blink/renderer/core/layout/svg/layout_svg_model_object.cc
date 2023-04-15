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
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"

namespace blink {

LayoutSVGModelObject::LayoutSVGModelObject(SVGElement* node)
    : LayoutObject(node) {}

bool LayoutSVGModelObject::IsChildAllowed(LayoutObject* child,
                                          const ComputedStyle&) const {
  NOT_DESTROYED();
  return SVGContentContainer::IsChildAllowed(*child);
}

void LayoutSVGModelObject::MapLocalToAncestor(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    MapCoordinatesFlags flags) const {
  NOT_DESTROYED();
  SVGLayoutSupport::MapLocalToAncestor(this, ancestor, transform_state, flags);
}

PhysicalRect LayoutSVGModelObject::VisualRectInDocument(
    VisualRectFlags flags) const {
  NOT_DESTROYED();
  return SVGLayoutSupport::VisualRectInAncestorSpace(*this, *View(), flags);
}

void LayoutSVGModelObject::MapAncestorToLocal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    MapCoordinatesFlags flags) const {
  NOT_DESTROYED();
  SVGLayoutSupport::MapAncestorToLocal(*this, ancestor, transform_state, flags);
}

void LayoutSVGModelObject::AbsoluteQuads(Vector<gfx::QuadF>& quads,
                                         MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  quads.push_back(LocalToAbsoluteQuad(gfx::QuadF(StrokeBoundingBox()), mode));
}

// This method is called from inside PaintOutline(), and since we call
// PaintOutline() while transformed to our coord system, return local coords.
void LayoutSVGModelObject::AddOutlineRects(OutlineRectCollector& collector,
                                           OutlineInfo* info,
                                           const PhysicalOffset&,
                                           NGOutlineType) const {
  NOT_DESTROYED();
  gfx::RectF visual_rect = VisualRectInLocalSVGCoordinates();
  bool was_empty = visual_rect.IsEmpty();
  SVGLayoutSupport::AdjustWithClipPathAndMask(*this, ObjectBoundingBox(),
                                              visual_rect);
  // If visual rect is clipped away then don't add it.
  if (!was_empty && visual_rect.IsEmpty())
    return;
  collector.AddRect(PhysicalRect::EnclosingRect(visual_rect));
  if (info)
    *info = OutlineInfo::GetUnzoomedFromStyle(StyleRef());
}

gfx::RectF LayoutSVGModelObject::LocalBoundingBoxRectForAccessibility() const {
  NOT_DESTROYED();
  return StrokeBoundingBox();
}

void LayoutSVGModelObject::WillBeDestroyed() {
  NOT_DESTROYED();
  SVGResources::ClearEffects(*this);
  LayoutObject::WillBeDestroyed();
}

AffineTransform LayoutSVGModelObject::CalculateLocalTransform() const {
  NOT_DESTROYED();
  auto* element = GetElement();
  if (element->HasTransform(SVGElement::kIncludeMotionTransform))
    return element->CalculateTransform(SVGElement::kIncludeMotionTransform);
  return AffineTransform();
}

bool LayoutSVGModelObject::CheckForImplicitTransformChange(
    bool bbox_changed) const {
  NOT_DESTROYED();
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
  NOT_DESTROYED();
  LayoutObject::StyleDidChange(diff, old_style);

  if (diff.NeedsFullLayout()) {
    SetNeedsBoundariesUpdate();
    if (diff.TransformChanged())
      SetNeedsTransformUpdate();
  }

  SetHasTransformRelatedProperty(
      StyleRef().HasTransformRelatedPropertyForSVG());

  SVGResources::UpdateEffects(*this, diff, old_style);

  if (!Parent())
    return;

  if (!IsSVGHiddenContainer()) {
    if (diff.BlendModeChanged()) {
      DCHECK(IsBlendingAllowed());
      Parent()->DescendantIsolationRequirementsChanged(
          StyleRef().HasBlendMode() ? kDescendantIsolationRequired
                                    : kDescendantIsolationNeedsUpdate);
    }
    if (StyleRef().HasCurrentTransformRelatedAnimation() &&
        !old_style->HasCurrentTransformRelatedAnimation()) {
      Parent()->SetSVGDescendantMayHaveTransformRelatedAnimation();
    }
  }

  if (diff.HasDifference())
    LayoutSVGResourceContainer::StyleChanged(*this, diff);
}

void LayoutSVGModelObject::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutObject::InsertedIntoTree();
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this,
                                                                         false);
  if (StyleRef().HasSVGEffect())
    SetNeedsPaintPropertyUpdate();
}

void LayoutSVGModelObject::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this,
                                                                         false);
  if (StyleRef().HasSVGEffect())
    SetNeedsPaintPropertyUpdate();
  LayoutObject::WillBeRemovedFromTree();
}

}  // namespace blink
