// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/svg_content_container.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_marker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"

namespace blink {

static void LayoutMarkerResourcesIfNeeded(LayoutObject& layout_object) {
  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(layout_object);
  if (!resources)
    return;
  if (LayoutSVGResourceMarker* marker = resources->MarkerStart())
    marker->LayoutIfNeeded();
  if (LayoutSVGResourceMarker* marker = resources->MarkerMid())
    marker->LayoutIfNeeded();
  if (LayoutSVGResourceMarker* marker = resources->MarkerEnd())
    marker->LayoutIfNeeded();
}

void SVGContentContainer::Layout(const SVGContainerLayoutInfo& layout_info) {
  for (LayoutObject* child = children_.FirstChild(); child;
       child = child->NextSibling()) {
    bool force_child_layout = layout_info.force_layout;

    if (layout_info.scale_factor_changed) {
      // If the screen scaling factor changed we need to update the text
      // metrics (note: this also happens for layoutSizeChanged=true).
      if (child->IsSVGText())
        To<LayoutSVGText>(child)->SetNeedsTextMetricsUpdate();
      force_child_layout = true;
    }

    if (layout_info.viewport_changed) {
      // When selfNeedsLayout is false and the layout size changed, we have to
      // check whether this child uses relative lengths
      if (auto* element = DynamicTo<SVGElement>(child->GetNode())) {
        if (element->HasRelativeLengths()) {
          // FIXME: this should be done on invalidation, not during layout.
          // When the layout size changed and when using relative values tell
          // the LayoutSVGShape to update its shape object
          if (child->IsSVGShape()) {
            To<LayoutSVGShape>(child)->SetNeedsShapeUpdate();
          } else if (child->IsSVGText()) {
            To<LayoutSVGText>(child)->SetNeedsTextMetricsUpdate();
            To<LayoutSVGText>(child)->SetNeedsPositioningValuesUpdate();
          }

          force_child_layout = true;
        }
      }
    }

    // Resource containers are nasty: they can invalidate clients outside the
    // current SubtreeLayoutScope.
    // Since they only care about viewport size changes (to resolve their
    // relative lengths), we trigger their invalidation directly from
    // SVGSVGElement::svgAttributeChange() or at a higher SubtreeLayoutScope (in
    // LayoutView::layout()). We do not create a SubtreeLayoutScope for
    // resources because their ability to reference each other leads to circular
    // layout. We protect against that within the layout code for marker
    // resources, but it causes assertions if we use a SubtreeLayoutScope for
    // them.
    if (child->IsSVGResourceContainer()) {
      child->LayoutIfNeeded();
    } else {
      SubtreeLayoutScope layout_scope(*child);
      if (force_child_layout) {
        layout_scope.SetNeedsLayout(child,
                                    layout_invalidation_reason::kSvgChanged);
      }

      // Lay out any referenced resources before the child.
      LayoutMarkerResourcesIfNeeded(*child);
      child->LayoutIfNeeded();
    }
  }
}

bool SVGContentContainer::HitTest(HitTestResult& result,
                                  const HitTestLocation& location,
                                  HitTestAction hit_test_action) const {
  PhysicalOffset accumulated_offset;
  for (LayoutObject* child = children_.LastChild(); child;
       child = child->PreviousSibling()) {
    if (auto* foreign_object = DynamicTo<LayoutSVGForeignObject>(child)) {
      if (foreign_object->NodeAtPointFromSVG(
              result, location, accumulated_offset, hit_test_action))
        return true;
    } else {
      if (child->NodeAtPoint(result, location, accumulated_offset,
                             hit_test_action))
        return true;
    }
  }
  return false;
}

// Update a bounding box taking into account the validity of the other bounding
// box.
static inline void UpdateObjectBoundingBox(FloatRect& object_bounding_box,
                                           bool& object_bounding_box_valid,
                                           FloatRect other_bounding_box) {
  if (!object_bounding_box_valid) {
    object_bounding_box = other_bounding_box;
    object_bounding_box_valid = true;
    return;
  }
  object_bounding_box.UniteEvenIfEmpty(other_bounding_box);
}

static bool HasValidBoundingBoxForContainer(const LayoutObject& object) {
  if (object.IsSVGShape())
    return !To<LayoutSVGShape>(object).IsShapeEmpty();

  if (object.IsSVGText())
    return To<LayoutSVGText>(object).IsObjectBoundingBoxValid();

  if (auto* svg_container = DynamicTo<LayoutSVGContainer>(object)) {
    return svg_container->IsObjectBoundingBoxValid() &&
           !svg_container->IsSVGHiddenContainer();
  }

  if (auto* foreign_object = DynamicTo<LayoutSVGForeignObject>(object))
    return foreign_object->IsObjectBoundingBoxValid();

  if (object.IsSVGImage())
    return To<LayoutSVGImage>(object).IsObjectBoundingBoxValid();

  return false;
}

bool SVGContentContainer::UpdateBoundingBoxes(bool& object_bounding_box_valid) {
  object_bounding_box_valid = false;

  FloatRect object_bounding_box;
  FloatRect stroke_bounding_box;
  for (LayoutObject* current = children_.FirstChild(); current;
       current = current->NextSibling()) {
    // Don't include elements that are not rendered.
    if (!HasValidBoundingBoxForContainer(*current))
      continue;
    const AffineTransform& transform = current->LocalToSVGParentTransform();
    UpdateObjectBoundingBox(object_bounding_box, object_bounding_box_valid,
                            transform.MapRect(current->ObjectBoundingBox()));
    stroke_bounding_box.Unite(transform.MapRect(current->StrokeBoundingBox()));
  }

  bool changed = false;
  changed |= object_bounding_box_ != object_bounding_box;
  object_bounding_box_ = object_bounding_box;
  changed |= stroke_bounding_box_ != stroke_bounding_box;
  stroke_bounding_box_ = stroke_bounding_box;
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

}  // namespace blink
