/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009, 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_area_element.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_map_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

namespace {

// Adapt a double to the allowed range of a LayoutUnit and narrow it to float
// precision.
float ClampCoordinate(double value) {
  return LayoutUnit(value).ToFloat();
}
}

HTMLAreaElement::HTMLAreaElement(Document& document)
    : HTMLAnchorElementBase(html_names::kAreaTag, document), shape_(kRect) {}

// An explicit empty destructor should be in html_area_element.cc, because
// if an implicit destructor is used or an empty destructor is defined in
// html_area_element.h, when including html_area_element.h, msvc tries to expand
// the destructor and causes a compile error because of lack of blink::Path
// definition.
HTMLAreaElement::~HTMLAreaElement() = default;

void HTMLAreaElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const AtomicString& value = params.new_value;
  if (params.name == html_names::kShapeAttr) {
    if (EqualIgnoringASCIICase(value, "default")) {
      shape_ = kDefault;
    } else if (EqualIgnoringASCIICase(value, "circle") ||
               EqualIgnoringASCIICase(value, "circ")) {
      shape_ = kCircle;
    } else if (EqualIgnoringASCIICase(value, "polygon") ||
               EqualIgnoringASCIICase(value, "poly")) {
      shape_ = kPoly;
    } else {
      // The missing (and implicitly invalid) value default for the
      // 'shape' attribute is 'rect'.
      shape_ = kRect;
    }
    InvalidateCachedPath();
  } else if (params.name == html_names::kCoordsAttr) {
    coords_ = ParseHTMLListOfFloatingPointNumbers(value.GetString());
    InvalidateCachedPath();
  } else if (params.name == html_names::kAltAttr ||
             params.name == html_names::kAccesskeyAttr) {
    // Do nothing.
  } else {
    HTMLAnchorElementBase::ParseAttribute(params);
  }
}

void HTMLAreaElement::InvalidateCachedPath() {
  path_ = nullptr;
}

bool HTMLAreaElement::PointInArea(const PhysicalOffset& location,
                                  const LayoutObject* container_object) const {
  return GetPath(container_object).Contains(gfx::PointF(location));
}

PhysicalRect HTMLAreaElement::ComputeAbsoluteRect(
    const LayoutObject* container_object) const {
  if (!container_object)
    return PhysicalRect();

  // FIXME: This doesn't work correctly with transforms.
  PhysicalOffset abs_pos = container_object->LocalToAbsolutePoint(
      PhysicalOffset(), kIgnoreTransforms);

  Path path = GetPath(container_object);
  path.Translate(gfx::Vector2dF(abs_pos));
  return PhysicalRect::EnclosingRect(path.BoundingRect());
}

Path HTMLAreaElement::GetPath(const LayoutObject* container_object) const {
  if (!container_object)
    return Path();

  // Always recompute for default shape because it depends on container object's
  // size and is cheap.
  if (shape_ == kDefault) {
    Path path;
    // No need to zoom because it is already applied in
    // container_object->PhysicalBorderBoxRect().
    if (const auto* box = DynamicTo<LayoutBox>(container_object))
      path.AddRect(gfx::RectF(box->PhysicalBorderBoxRect()));
    path_ = nullptr;
    return path;
  }

  Path path;
  if (path_) {
    path = *path_;
  } else {
    if (coords_.empty())
      return path;

    switch (shape_) {
      case kPoly:
        if (coords_.size() >= 6) {
          int num_points = coords_.size() / 2;
          path.MoveTo(gfx::PointF(ClampCoordinate(coords_[0]),
                                  ClampCoordinate(coords_[1])));
          for (int i = 1; i < num_points; ++i) {
            path.AddLineTo(gfx::PointF(ClampCoordinate(coords_[i * 2]),
                                       ClampCoordinate(coords_[i * 2 + 1])));
          }
          path.CloseSubpath();
          path.SetWindRule(RULE_EVENODD);
        }
        break;
      case kCircle:
        if (coords_.size() >= 3 && coords_[2] > 0) {
          float r = ClampCoordinate(coords_[2]);
          path.AddEllipse(gfx::PointF(ClampCoordinate(coords_[0]),
                                      ClampCoordinate(coords_[1])),
                          r, r);
        }
        break;
      case kRect:
        if (coords_.size() >= 4) {
          float x0 = ClampCoordinate(coords_[0]);
          float y0 = ClampCoordinate(coords_[1]);
          float x1 = ClampCoordinate(coords_[2]);
          float y1 = ClampCoordinate(coords_[3]);
          path.AddRect(gfx::PointF(x0, y0), gfx::PointF(x1, y1));
        }
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    // Cache the original path, not depending on containerObject.
    path_ = std::make_unique<Path>(path);
  }

  // Zoom the path into coordinates of the container object.
  float zoom_factor = container_object->StyleRef().EffectiveZoom();
  if (zoom_factor != 1.0f) {
    AffineTransform zoom_transform;
    zoom_transform.Scale(zoom_factor);
    path.Transform(zoom_transform);
  }
  return path;
}

HTMLImageElement* HTMLAreaElement::ImageElement() const {
  if (HTMLMapElement* map_element =
          Traversal<HTMLMapElement>::FirstAncestor(*this))
    return map_element->ImageElement();
  return nullptr;
}

bool HTMLAreaElement::IsKeyboardFocusable(
    UpdateBehavior update_behavior) const {
  // Explicitly skip over the HTMLAnchorElementBase's keyboard focus behavior.
  return Element::IsKeyboardFocusable(update_behavior);
}

FocusableState HTMLAreaElement::IsFocusableState(
    UpdateBehavior update_behavior) const {
  // Explicitly skip over the HTMLAnchorElementBase's mouse focus behavior.
  return HTMLElement::IsFocusableState(update_behavior);
}

bool HTMLAreaElement::IsFocusableStyle(UpdateBehavior update_behavior) const {
  HTMLImageElement* image = ImageElement();
  if (!image) {
    return false;
  }
  LayoutObject* layout_object = image->GetLayoutObject();
  if (!layout_object) {
    return false;
  }
  const ComputedStyle& style = layout_object->StyleRef();
  // TODO(crbug.com/40911863): Why is this not just image->IsFocusableStyle()?
  return !style.IsInert() && style.UsedVisibility() == EVisibility::kVisible &&
         Element::tabIndex() >= 0 &&
         SupportsFocus(update_behavior) != FocusableState::kNotFocusable;
}

void HTMLAreaElement::SetFocused(bool should_be_focused,
                                 mojom::blink::FocusType focus_type) {
  if (IsFocused() == should_be_focused)
    return;

  HTMLAnchorElementBase::SetFocused(should_be_focused, focus_type);

  HTMLImageElement* image_element = ImageElement();
  if (!image_element)
    return;

  LayoutObject* layout_object = image_element->GetLayoutObject();
  if (auto* layout_image = DynamicTo<LayoutImage>(layout_object))
    layout_image->AreaElementFocusChanged(this);
}

Element* HTMLAreaElement::interestTargetElement() {
  CHECK(RuntimeEnabledFeatures::HTMLInterestTargetAttributeEnabled());

  if (!IsInTreeScope()) {
    return nullptr;
  }

  return GetElementAttributeResolvingReferenceTarget(
      html_names::kInteresttargetAttr);
}

AtomicString HTMLAreaElement::interestAction() const {
  CHECK(RuntimeEnabledFeatures::HTMLInterestTargetAttributeEnabled());
  const AtomicString& attribute_value =
      FastGetAttribute(html_names::kInterestactionAttr);
  if (attribute_value && !attribute_value.IsNull() &&
      !attribute_value.empty()) {
    return attribute_value;
  }
  return g_empty_atom;
}

void HTMLAreaElement::UpdateSelectionOnFocus(
    SelectionBehaviorOnFocus selection_behavior,
    const FocusOptions* options) {
  GetDocument().UpdateStyleAndLayoutTreeForElement(
      this, DocumentUpdateReason::kFocus);
  if (!IsFocusable())
    return;

  if (HTMLImageElement* image_element = ImageElement()) {
    image_element->UpdateSelectionOnFocus(selection_behavior, options);
  }
}

}  // namespace blink
