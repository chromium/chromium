// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/svg_content_container.h"

#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_marker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_info.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"

namespace blink {

namespace {

bool UpdateSVGLayoutIfNeeded(LayoutObject* child,
                             const SVGLayoutInfo& layout_info) {
  if (layout_info.viewport_changed && child->HasViewportDependence()) {
    child->SetNeedsLayout(layout_invalidation_reason::kSvgChanged,
                          kMarkOnlyThis);
  }
  if (child->NeedsLayout()) {
    const SVGLayoutResult child_result = child->UpdateSVGLayout(layout_info);
    child->SetHasViewportDependence(child_result.has_viewport_dependence);
  }
  return child->HasViewportDependence();
}

bool LayoutMarkerResourcesIfNeeded(LayoutObject& layout_object,
                                   const SVGLayoutInfo& layout_info) {
  SVGElementResourceClient* client = SVGResources::GetClient(layout_object);
  if (!client)
    return false;
  bool has_viewport_dependence = false;
  const ComputedStyle& style = layout_object.StyleRef();
  if (auto* marker = GetSVGResourceAsType<LayoutSVGResourceMarker>(
          *client, style.MarkerStartResource()))
    has_viewport_dependence |= UpdateSVGLayoutIfNeeded(marker, layout_info);
  if (auto* marker = GetSVGResourceAsType<LayoutSVGResourceMarker>(
          *client, style.MarkerMidResource()))
    has_viewport_dependence |= UpdateSVGLayoutIfNeeded(marker, layout_info);
  if (auto* marker = GetSVGResourceAsType<LayoutSVGResourceMarker>(
          *client, style.MarkerEndResource()))
    has_viewport_dependence |= UpdateSVGLayoutIfNeeded(marker, layout_info);
  return has_viewport_dependence;
}

// Update a bounding box taking into account the validity of the other bounding
// box.
inline void UpdateObjectBoundingBox(gfx::RectF& object_bounding_box,
                                    bool& object_bounding_box_valid,
                                    const gfx::RectF& other_bounding_box) {
  if (!object_bounding_box_valid) {
    object_bounding_box = other_bounding_box;
    object_bounding_box_valid = true;
    return;
  }
  object_bounding_box.UnionEvenIfEmpty(other_bounding_box);
}

bool HasValidBoundingBoxForContainer(const LayoutObject& object) {
  if (auto* svg_shape = DynamicTo<LayoutSVGShape>(object)) {
    return !svg_shape->IsShapeEmpty();
  }
  if (auto* text = DynamicTo<LayoutSVGText>(object)) {
    return text->IsObjectBoundingBoxValid();
  }
  if (auto* svg_container = DynamicTo<LayoutSVGContainer>(object)) {
    return svg_container->IsObjectBoundingBoxValid() &&
           !svg_container->IsSVGHiddenContainer();
  }
  if (auto* foreign_object = DynamicTo<LayoutSVGForeignObject>(object)) {
    return foreign_object->IsObjectBoundingBoxValid();
  }
  if (auto* svg_image = DynamicTo<LayoutSVGImage>(object)) {
    return svg_image->IsObjectBoundingBoxValid();
  }
  return false;
}

gfx::RectF ObjectBoundsForPropagation(const LayoutObject& object) {
  gfx::RectF bounds = object.ObjectBoundingBox();
  // The local-to-parent transform for <foreignObject> contains a zoom inverse,
  // so we need to apply zoom to the bounding box that we use for propagation to
  // be in the correct coordinate space.
  if (object.IsSVGForeignObject()) {
    bounds.Scale(object.StyleRef().EffectiveZoom());
  }
  return bounds;
}

bool ShouldForceLayoutChild(const SVGLayoutInfo& layout_info,
                            const LayoutObject& child) {
  if (layout_info.force_layout) {
    return true;
  }
  if (layout_info.scale_factor_changed) {
    return true;
  }
  if (layout_info.viewport_changed && child.HasViewportDependence()) {
    return true;
  }
  return false;
}

}  // namespace

// static
bool SVGContentContainer::IsChildAllowed(const LayoutObject& child) {
  // https://svgwg.org/svg2-draft/struct.html#ForeignNamespaces
  // the SVG user agent must include the unknown foreign-namespaced elements
  // in the DOM but will ignore and exclude them for rendering purposes.
  if (!child.IsSVG())
    return false;
  if (child.IsSVGInline() || child.IsSVGInlineText())
    return false;
  // The above IsSVG() check is not enough for a <svg> in a foreign element
  // with `display: contents` because SVGSVGElement::LayoutObjectIsNeeded()
  // doesn't check HasSVGParent().
  return !child.IsSVGRoot();
}

SVGLayoutResult SVGContentContainer::Layout(const SVGLayoutInfo& layout_info) {
  bool bounds_changed = std::exchange(bounds_dirty_from_removed_child_, false);
  bool has_viewport_dependence = false;

  for (LayoutObject* child = children_.FirstChild(); child;
       child = child->NextSibling()) {
    DCHECK(!child->IsSVGRoot());
    if (ShouldForceLayoutChild(layout_info, *child)) {
      child->SetNeedsLayout(layout_invalidation_reason::kSvgChanged,
                            kMarkOnlyThis);
    }

    // Lay out any referenced resources before the child.
    has_viewport_dependence |=
        LayoutMarkerResourcesIfNeeded(*child, layout_info);

    if (child->NeedsLayout()) {
      const SVGLayoutResult child_result = child->UpdateSVGLayout(layout_info);
      bounds_changed |= child_result.bounds_changed;
      child->SetHasViewportDependence(child_result.has_viewport_dependence);
    }

    has_viewport_dependence |= child->HasViewportDependence();
  }

  if (bounds_changed) {
    bounds_changed = UpdateBoundingBoxes();
  }
  return SVGLayoutResult(bounds_changed, has_viewport_dependence);
}

bool SVGContentContainer::HitTest(HitTestResult& result,
                                  const HitTestLocation& location,
                                  HitTestPhase phase) const {
  PhysicalOffset accumulated_offset;
  for (LayoutObject* child = children_.LastChild(); child;
       child = child->PreviousSibling()) {
    if (auto* foreign_object = DynamicTo<LayoutSVGForeignObject>(child)) {
      if (foreign_object->NodeAtPointFromSVG(result, location,
                                             accumulated_offset, phase)) {
        return true;
      }
    } else {
      if (child->NodeAtPoint(result, location, accumulated_offset, phase))
        return true;
    }
  }
  return false;
}

bool SVGContentContainer::UpdateBoundingBoxes() {
  object_bounding_box_valid_ = false;

  gfx::RectF object_bounding_box;
  gfx::RectF decorated_bounding_box;
  for (LayoutObject* current = children_.FirstChild(); current;
       current = current->NextSibling()) {
    // Don't include elements that are not rendered.
    if (!HasValidBoundingBoxForContainer(*current))
      continue;
    const AffineTransform& transform = current->LocalToSVGParentTransform();
    UpdateObjectBoundingBox(
        object_bounding_box, object_bounding_box_valid_,
        transform.MapRect(ObjectBoundsForPropagation(*current)));
    decorated_bounding_box.Union(
        transform.MapRect(current->DecoratedBoundingBox()));
  }

  bool changed = false;
  changed |= object_bounding_box_ != object_bounding_box;
  object_bounding_box_ = object_bounding_box;
  changed |= decorated_bounding_box_ != decorated_bounding_box;
  decorated_bounding_box_ = decorated_bounding_box;
  return changed;
}

bool SVGContentContainer::ComputeHasNonIsolatedBlendingDescendants() const {
  for (LayoutObject* child = children_.FirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsBlendingAllowed() && child->StyleRef().HasBlendMode())
      return true;
    if (child->HasNonIsolatedBlendingDescendants() &&
        !SVGLayoutSupport::WillIsolateBlendingDescendantsForObject(child))
      return true;
  }
  return false;
}

gfx::RectF SVGContentContainer::ComputeStrokeBoundingBox() const {
  gfx::RectF stroke_bbox;
  for (LayoutObject* child = children_.FirstChild(); child;
       child = child->NextSibling()) {
    // Don't include elements that are not rendered.
    if (!HasValidBoundingBoxForContainer(*child)) {
      continue;
    }
    const AffineTransform& transform = child->LocalToSVGParentTransform();
    stroke_bbox.Union(transform.MapRect(child->StrokeBoundingBox()));
  }
  return stroke_bbox;
}

}  // namespace blink
