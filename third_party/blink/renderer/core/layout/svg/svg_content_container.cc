// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/svg_content_container.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"

namespace blink {

void SVGContentContainer::Layout(bool force_layout,
                                 bool screen_scaling_factor_changed,
                                 bool layout_size_changed) {
  for (LayoutObject* child = children_.FirstChild(); child;
       child = child->NextSibling()) {
    bool force_child_layout = force_layout;

    if (screen_scaling_factor_changed) {
      // If the screen scaling factor changed we need to update the text
      // metrics (note: this also happens for layoutSizeChanged=true).
      if (child->IsSVGText())
        ToLayoutSVGText(child)->SetNeedsTextMetricsUpdate();
      force_child_layout = true;
    }

    if (layout_size_changed) {
      // When selfNeedsLayout is false and the layout size changed, we have to
      // check whether this child uses relative lengths
      if (auto* element = DynamicTo<SVGElement>(child->GetNode())) {
        if (element->HasRelativeLengths()) {
          // FIXME: this should be done on invalidation, not during layout.
          // When the layout size changed and when using relative values tell
          // the LayoutSVGShape to update its shape object
          if (child->IsSVGShape()) {
            ToLayoutSVGShape(child)->SetNeedsShapeUpdate();
          } else if (child->IsSVGText()) {
            ToLayoutSVGText(child)->SetNeedsTextMetricsUpdate();
            ToLayoutSVGText(child)->SetNeedsPositioningValuesUpdate();
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
    // layout. We protect against that within the layout code for resources, but
    // it causes assertions if we use a SubTreeLayoutScope for them.
    if (child->IsSVGResourceContainer()) {
      // Lay out any referenced resources before the child.
      SVGLayoutSupport::LayoutResourcesIfNeeded(*child);
      child->LayoutIfNeeded();
    } else {
      SubtreeLayoutScope layout_scope(*child);
      if (force_child_layout) {
        layout_scope.SetNeedsLayout(child,
                                    layout_invalidation_reason::kSvgChanged);
      }

      // Lay out any referenced resources before the child.
      SVGLayoutSupport::LayoutResourcesIfNeeded(*child);
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
                                           LayoutObject* other,
                                           FloatRect other_bounding_box) {
  auto* svg_container = DynamicTo<LayoutSVGContainer>(other);
  bool other_valid =
      svg_container ? svg_container->IsObjectBoundingBoxValid() : true;
  if (!other_valid)
    return;

  if (!object_bounding_box_valid) {
    object_bounding_box = other_bounding_box;
    object_bounding_box_valid = true;
    return;
  }

  object_bounding_box.UniteEvenIfEmpty(other_bounding_box);
}

static bool HasValidBoundingBoxForContainer(const LayoutObject* object) {
  if (object->IsSVGShape())
    return !ToLayoutSVGShape(object)->IsShapeEmpty();

  if (object->IsSVGText())
    return ToLayoutSVGText(object)->IsObjectBoundingBoxValid();

  if (object->IsSVGHiddenContainer())
    return false;

  if (auto* foreign_object = DynamicTo<LayoutSVGForeignObject>(object))
    return foreign_object->IsObjectBoundingBoxValid();

  if (object->IsSVGImage())
    return ToLayoutSVGImage(object)->IsObjectBoundingBoxValid();

  // TODO(fs): Can we refactor this code to include the container case
  // in a more natural way?
  return true;
}

void SVGContentContainer::ComputeBoundingBoxes(
    FloatRect& object_bounding_box,
    bool& object_bounding_box_valid,
    FloatRect& stroke_bounding_box) const {
  object_bounding_box = FloatRect();
  object_bounding_box_valid = false;
  stroke_bounding_box = FloatRect();

  // When computing the strokeBoundingBox, we use the visualRects of
  // the container's children so that the container's stroke includes the
  // resources applied to the children (such as clips and filters). This allows
  // filters applied to containers to correctly bound the children, and also
  // improves inlining of SVG content, as the stroke bound is used in that
  // situation also.
  for (LayoutObject* current = children_.FirstChild(); current;
       current = current->NextSibling()) {
    // Don't include elements that are not rendered in the union.
    if (!HasValidBoundingBoxForContainer(current))
      continue;

    const AffineTransform& transform = current->LocalToSVGParentTransform();
    UpdateObjectBoundingBox(object_bounding_box, object_bounding_box_valid,
                            current,
                            transform.MapRect(current->ObjectBoundingBox()));
    stroke_bounding_box.Unite(
        transform.MapRect(current->VisualRectInLocalSVGCoordinates()));
  }
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
