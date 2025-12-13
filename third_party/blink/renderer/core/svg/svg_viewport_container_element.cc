// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_viewport_container_element.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_animated_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_animated_rect.h"

namespace blink {

SVGViewportContainerElement::SVGViewportContainerElement(
    const QualifiedName& tag_name,
    Document& document)
    : SVGGraphicsElement(tag_name, document),
      SVGFitToViewBox(this),
      x_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kXAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kX)),
      y_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kYAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kY)),
      width_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kWidthAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kPercent100,
          CSSPropertyID::kWidth)),
      height_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kHeightAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kPercent100,
          CSSPropertyID::kHeight)) {}

const SVGRect& SVGViewportContainerElement::CurrentViewBox() const {
  return *viewBox()->CurrentValue();
}

gfx::RectF SVGViewportContainerElement::CurrentViewBoxRect() const {
  gfx::RectF use_view_box = CurrentViewBox().Rect();
  if (!use_view_box.IsEmpty()) {
    return use_view_box;
  }
  return gfx::RectF();
}

AffineTransform SVGViewportContainerElement::ViewBoxToViewTransform(
    const gfx::SizeF& viewport_size) const {
  return SVGFitToViewBox::ViewBoxToViewTransform(
      CurrentViewBoxRect(), CurrentPreserveAspectRatio(), viewport_size);
}

bool SVGViewportContainerElement::HasEmptyViewBox() const {
  const SVGRect& view_box = CurrentViewBox();
  return HasValidViewBox(view_box) && view_box.Rect().IsEmpty();
}

void SVGViewportContainerElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{x_.Get(), y_.Get(), width_.Get(),
                                   height_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGFitToViewBox::SynchronizeAllSVGAttributes();
  SVGGraphicsElement::SynchronizeAllSVGAttributes();
}

SVGAnimatedPropertyBase* SVGViewportContainerElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kXAttr) {
    return x_.Get();
  }
  if (attribute_name == svg_names::kYAttr) {
    return y_.Get();
  }
  if (attribute_name == svg_names::kWidthAttr) {
    return width_.Get();
  }
  if (attribute_name == svg_names::kHeightAttr) {
    return height_.Get();
  }
  if (SVGAnimatedPropertyBase* ret =
          SVGFitToViewBox::PropertyFromAttribute(attribute_name)) {
    return ret;
  }
  return SVGGraphicsElement::PropertyFromAttribute(attribute_name);
}

void SVGViewportContainerElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  bool update_relative_lengths_or_view_box = false;
  bool width_or_height_changed =
      attr_name == svg_names::kWidthAttr || attr_name == svg_names::kHeightAttr;
  if (width_or_height_changed || attr_name == svg_names::kXAttr ||
      attr_name == svg_names::kYAttr) {
    update_relative_lengths_or_view_box = true;

    // At the SVG/HTML boundary (aka LayoutSVGRoot), the width and
    // height attributes can affect the replaced size so we need
    // to mark it for updating.
    if (width_or_height_changed) {
      LayoutObject* layout_object = GetLayoutObject();
      // If the element is not attached, we cannot be sure if it is (going to
      // be) an outermost root, so always mark presentation attributes dirty in
      // that case.
      if (!layout_object || layout_object->IsSVGRoot()) {
        UpdatePresentationAttributeStyle(params.property);
        if (layout_object) {
          To<LayoutSVGRoot>(layout_object)->IntrinsicSizingInfoChanged();
        }
      } else if (RuntimeEnabledFeatures::
                     CollectWidthAndHeightAsStylesForNestedSvgEnabled()) {
        UpdatePresentationAttributeStyle(params.property);
      }
    } else {
      UpdatePresentationAttributeStyle(params.property);
    }
  }

  if (SVGFitToViewBox::IsKnownAttribute(attr_name)) {
    update_relative_lengths_or_view_box = true;
    if (LayoutObject* object = GetLayoutObject()) {
      object->SetNeedsTransformUpdate();
      if (attr_name == svg_names::kViewBoxAttr && object->IsSVGRoot()) {
        To<LayoutSVGRoot>(object)->IntrinsicSizingInfoChanged();
      }
    }
  }

  if (update_relative_lengths_or_view_box) {
    if (auto* layout_object = GetLayoutObject()) {
      MarkForLayoutAndParentResourceInvalidation(*layout_object);
    }
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(params);
}

bool SVGViewportContainerElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative() ||
         SVGGraphicsElement::SelfHasRelativeLengths();
}

const SVGPreserveAspectRatio*
SVGViewportContainerElement::CurrentPreserveAspectRatio() const {
  return preserveAspectRatio()->CurrentValue();
}

void SVGViewportContainerElement::Trace(Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  SVGGraphicsElement::Trace(visitor);
  SVGFitToViewBox::Trace(visitor);
}

}  // namespace blink
