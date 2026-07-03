/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Google, Inc.
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

#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_matrix_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_rect_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_symbol_element.h"
#include "third_party/blink/renderer/core/svg/svg_zoom_migration.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

namespace {

bool HasValidBoundingBoxForContainer(const LayoutObject* object) {
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

}  // namespace

SVGGraphicsElement::SVGGraphicsElement(const QualifiedName& tag_name,
                                       Document& document,
                                       ConstructionType construction_type)
    : SVGTransformableElement(tag_name, document, construction_type) {}

SVGGraphicsElement::~SVGGraphicsElement() = default;

void SVGGraphicsElement::Trace(Visitor* visitor) const {
  visitor->Trace(tests_);
  SVGTransformableElement::Trace(visitor);
}

// TODO : This function performs an upward traversal of the layout tree to check
// if the element is inside a `LayoutSVGHiddenContainer`, this is not very
// efficient. Consider using a cache based system where each svg element (or its
// corresponding layout object) has a flag that indicates if it is inside a
// `LayoutSVGHiddenContainer`.
bool SVGGraphicsElement::IsNonRendered(const LayoutObject* object) const {
  for (; object; object = object->Parent()) {
    // Check if the Element's LayoutObject or any ancestor is a
    // LayoutSVGHiddenContainer
    if (object->IsSVGHiddenContainer()) {
      return true;
    }

    if (IsA<LayoutSVGRoot>(*object)) {
      break;
    }
  }
  return false;
}

static bool IsViewportElement(const Element& element) {
  return (IsA<SVGSVGElement>(element) || IsA<SVGSymbolElement>(element) ||
          IsA<SVGForeignObjectElement>(element) ||
          IsA<SVGImageElement>(element));
}

AffineTransform SVGGraphicsElement::ComputeCTM(
    SVGElement::CTMScope mode,
    const SVGGraphicsElement* ancestor) const {
  AffineTransform ctm;
  bool done = false;

  for (const Element* current_element = this; current_element && !done;
       current_element = current_element->ParentOrShadowHostElement()) {
    auto* svg_element = DynamicTo<SVGElement>(current_element);
    if (!svg_element)
      break;

    AffineTransform local = svg_element->LocalCoordinateSpaceTransform(mode);
    if (RuntimeEnabledFeatures::SvgNewZoomEnabled() && mode == kScreenScope) {
      // TODO(crbug.com/530378493): This per-element un-zoom (and the outermost
      // SVG special-case below) exists because
      // SVGSVGElement::LocalCoordinateSpaceTransform() still adjusts for zoom.
      // It goes away once getScreenCTM() applies the un-zoom in the caller and
      // that helper returns an unadjusted matrix.
      // Under SvgNewZoom, descendant LocalCoordinateSpaceTransform()s
      // contribute translations in zoomed CSS pixels (because
      // `SVGLength.Value(context)` and transform_helper.cc::ComputeTransform
      // both fold in EffectiveZoom), while getScreenCTM must return the matrix
      // in SVG user-space units. We can't just un-zoom the final composed
      // matrix here because the outermost SVG element contributes a transform
      // already in absolute screen-pixel coordinates (it combines
      // LocalToAbsoluteTransform with LocalToBorderBoxTransform) that must not
      // be un-zoomed, whereas the inner elements must be. So inverse-scale the
      // inner elements per-element and leave the outermost SVG alone.
      //
      // (kNearestViewportScope has no such screen-pixel element, so getCTM()
      // un-zooms the composed matrix once in the caller. kAncestorScope is used
      // internally to map zoomed-CSS-pixel layout rects, e.g.
      // VisualRectInLocalSVGCoordinates, so it keeps the zoom factor baked in.)
      const auto* outer_svg = DynamicTo<SVGSVGElement>(svg_element);
      const bool is_outer_svg =
          outer_svg && outer_svg->IsOutermostSVGSVGElement();
      if (!is_outer_svg) {
        if (const ComputedStyle* style = svg_element->GetComputedStyle()) {
          const float zoom = style->EffectiveZoom();
          if (zoom != 1.0f) {
            local.SetE(local.E() / zoom);
            local.SetF(local.F() / zoom);
          }
        }
      }
    }
    ctm = local.PreConcat(ctm);

    switch (mode) {
      case kNearestViewportScope:
        // Stop at the nearest viewport ancestor.
        done = current_element != this && IsViewportElement(*current_element);
        break;
      case kAncestorScope:
        // Stop at the designated ancestor.
        done = current_element == ancestor;
        break;
      default:
        DCHECK_EQ(mode, kScreenScope);
        break;
    }
  }
  return ctm;
}

SVGMatrixTearOff* SVGGraphicsElement::getCTM() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  AffineTransform ctm = ComputeCTM(kNearestViewportScope);
  if (RuntimeEnabledFeatures::SvgNewZoomEnabled()) {
    // ComputeCTM(kNearestViewportScope) composes per-element transforms whose
    // translations are in zoomed CSS pixels; getCTM must return SVG user-space
    // units. Every element shares the page zoom and translation composition is
    // linear in the per-element translations, so un-zooming the composed
    // translation once is equivalent to un-zooming each element.
    if (const ComputedStyle* style = GetComputedStyle()) {
      ctm.Zoom(1 / style->EffectiveZoom());
    }
  }
  return MakeGarbageCollected<SVGMatrixTearOff>(ctm);
}

SVGMatrixTearOff* SVGGraphicsElement::getScreenCTM() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  return MakeGarbageCollected<SVGMatrixTearOff>(ComputeCTM(kScreenScope));
}

void SVGGraphicsElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  // Reattach so the isValid() check will be run again during layoutObject
  // creation.
  if (SVGTests::IsKnownAttribute(attr_name)) {
    SetForceReattachLayoutTree();
    return;
  }
  SVGTransformableElement::SvgAttributeChanged(params);
}

SVGElement* SVGGraphicsElement::nearestViewportElement() const {
  for (Element* current = ParentOrShadowHostElement(); current;
       current = current->ParentOrShadowHostElement()) {
    if (IsViewportElement(*current))
      return To<SVGElement>(current);
  }

  return nullptr;
}

SVGElement* SVGGraphicsElement::farthestViewportElement() const {
  SVGElement* farthest = nullptr;
  for (Element* current = ParentOrShadowHostElement(); current;
       current = current->ParentOrShadowHostElement()) {
    if (IsViewportElement(*current))
      farthest = To<SVGElement>(current);
  }
  return farthest;
}

gfx::RectF SVGGraphicsElement::GetBBox() {
  auto* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  gfx::RectF bbox = layout_object->ObjectBoundingBox();

  if (bbox != gfx::RectF(0, 0, 0, 0) &&
      (!HasValidBoundingBoxForContainer(layout_object) ||
       IsNonRendered(layout_object))) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kGetBBoxForElementWithZeroWidthOrHeight);
  }

  // TODO: Having zero width or height in viewport makes the container element
  // non-rendered, currently we return the bbox of its contents, which can
  // change if we decided to return an empty bbox for such cases. Depending on
  // the count of `kGetBBoxForNestedSVGElementWithZeroWidthOrHeight` it might be
  // safe to not consider this case at all.
  if (auto* svg_viewport_container =
          DynamicTo<LayoutSVGViewportContainer>(layout_object)) {
    if (bbox != gfx::RectF(0, 0, 0, 0) &&
        svg_viewport_container->Viewport().IsEmpty()) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::kGetBBoxForNestedSVGElementWithZeroWidthOrHeight);
    }
  }

  return NoopWillBeInvScaleRect(bbox,
                                layout_object->StyleRef().EffectiveZoom());
}

SVGRectTearOff* SVGGraphicsElement::getBBoxFromJavascript() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  // FIXME: Eventually we should support getBBox for detached elements.
  gfx::RectF bounding_box;
  if (const auto* layout_object = GetLayoutObject()) {
    bounding_box = GetBBox();

    if (layout_object->IsSVGInline()) {
      UseCounter::Count(GetDocument(), WebFeature::kGetBBoxForText);
    }
  }
  return SVGRectTearOff::CreateDetached(bounding_box);
}

SVGAnimatedPropertyBase* SVGGraphicsElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (SVGTests::IsKnownAttribute(attribute_name)) {
    return EnsureSvgTests().PropertyFromAttribute(this, attribute_name);
  }
  return SVGTransformableElement::PropertyFromAttribute(attribute_name);
}

void SVGGraphicsElement::SynchronizeAllSVGAttributes() const {
  if (tests_) {
    tests_->SynchronizeAllSVGAttributes();
  }
  SVGTransformableElement::SynchronizeAllSVGAttributes();
}

SVGStringListTearOff* SVGGraphicsElement::requiredExtensions() {
  return EnsureSvgTests().requiredExtensions(this);
}

SVGStringListTearOff* SVGGraphicsElement::systemLanguage() {
  return EnsureSvgTests().systemLanguage(this);
}

SVGTests& SVGGraphicsElement::EnsureSvgTests() const {
  if (!tests_) {
    tests_ = MakeGarbageCollected<SVGTests>();
  }
  return *tests_;
}

}  // namespace blink
