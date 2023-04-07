/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_image_element.h"

#include "third_party/blink/public/mojom/permissions_policy/document_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_animated_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

SVGImageElement::SVGImageElement(Document& document)
    : SVGGraphicsElement(svg_names::kImageTag, document),
      SVGURIReference(this),
      ActiveScriptWrappable<SVGImageElement>({}),
      is_default_overridden_intrinsic_size_(
          GetExecutionContext() &&
          !GetExecutionContext()->IsFeatureEnabled(
              mojom::blink::DocumentPolicyFeature::kUnsizedMedia)),
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
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kWidth)),
      height_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kHeightAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kHeight)),
      preserve_aspect_ratio_(
          MakeGarbageCollected<SVGAnimatedPreserveAspectRatio>(
              this,
              svg_names::kPreserveAspectRatioAttr)),
      image_loader_(MakeGarbageCollected<SVGImageLoader>(this)) {
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);
  AddToPropertyMap(preserve_aspect_ratio_);
}

void SVGImageElement::Trace(Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(preserve_aspect_ratio_);
  visitor->Trace(image_loader_);
  SVGGraphicsElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

bool SVGImageElement::CurrentFrameHasSingleSecurityOrigin() const {
  if (auto* layout_svg_image = To<LayoutSVGImage>(GetLayoutObject())) {
    LayoutImageResource* layout_image_resource =
        layout_svg_image->ImageResource();
    ImageResourceContent* image_content = layout_image_resource->CachedImage();
    if (image_content) {
      if (Image* image = image_content->GetImage())
        return image->CurrentFrameHasSingleSecurityOrigin();
    }
  }
  return true;
}

ScriptPromise SVGImageElement::decode(ScriptState* script_state,
                                      ExceptionState& exception_state) {
  return GetImageLoader().Decode(script_state, exception_state);
}

void SVGImageElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property == width_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            width_->CssValue());
  } else if (property == height_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            height_->CssValue());
  } else if (property == x_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            x_->CssValue());
  } else if (property == y_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            y_->CssValue());
  } else {
    SVGGraphicsElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void SVGImageElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  bool is_length_attribute =
      attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kWidthAttr || attr_name == svg_names::kHeightAttr;

  if (is_length_attribute || attr_name == svg_names::kPreserveAspectRatioAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    if (is_length_attribute) {
      InvalidateSVGPresentationAttributeStyle();
      SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::FromAttribute(attr_name));
      UpdateRelativeLengthsInformation();
    }

    LayoutObject* object = GetLayoutObject();
    if (!object)
      return;

    // FIXME: if isLengthAttribute then we should avoid this call if the
    // viewport didn't change, however since we don't have the computed
    // style yet we can't use updateBoundingBox/updateImageContainerSize.
    // See http://crbug.com/466200.
    MarkForLayoutAndParentResourceInvalidation(*object);
    return;
  }

  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    GetImageLoader().UpdateFromElement(ImageLoader::kUpdateIgnorePreviousError);
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(params);
}

void SVGImageElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == svg_names::kDecodingAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kImageDecodingAttribute);
    decoding_mode_ = ParseImageDecodingMode(params.new_value);
  } else {
    SVGElement::ParseAttribute(params);
  }
}

bool SVGImageElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative();
}

LayoutObject* SVGImageElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutSVGImage>(this);
}

bool SVGImageElement::HaveLoadedRequiredResources() {
  return !GetImageLoader().HasPendingActivity();
}

void SVGImageElement::AttachLayoutTree(AttachContext& context) {
  SVGGraphicsElement::AttachLayoutTree(context);

  if (auto* image_obj = To<LayoutSVGImage>(GetLayoutObject())) {
    LayoutImageResource* layout_image_resource = image_obj->ImageResource();
    if (layout_image_resource->HasImage())
      return;
    layout_image_resource->SetImageResource(GetImageLoader().GetContent());
  }
}

const AtomicString SVGImageElement::ImageSourceURL() const {
  return AtomicString(HrefString());
}

void SVGImageElement::DidMoveToNewDocument(Document& old_document) {
  GetImageLoader().ElementDidMoveToNewDocument();
  SVGGraphicsElement::DidMoveToNewDocument(old_document);
  GetImageLoader().UpdateFromElement(ImageLoader::kUpdateIgnorePreviousError);
}

}  // namespace blink
