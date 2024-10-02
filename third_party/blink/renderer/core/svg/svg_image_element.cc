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

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
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
      image_loader_(MakeGarbageCollected<SVGImageLoader>(this)) {}

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

ScriptPromise<IDLUndefined> SVGImageElement::decode(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return GetImageLoader().Decode(script_state, exception_state);
}

void SVGImageElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  bool is_length_attribute =
      attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kWidthAttr || attr_name == svg_names::kHeightAttr;

  if (is_length_attribute || attr_name == svg_names::kPreserveAspectRatioAttr) {
    if (is_length_attribute) {
      UpdatePresentationAttributeStyle(params.property);
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
  } else if (params.name == html_names::kCrossoriginAttr) {
    // As per an image's relevant mutations [1], we must queue a new loading
    // microtask when the `crossorigin` attribute state has changed. Note that
    // the attribute value can change without the attribute state changing [2].
    //
    // [1]:
    // https://html.spec.whatwg.org/multipage/images.html#relevant-mutations
    // [2]: https://github.com/whatwg/html/issues/4533#issuecomment-483417499
    CrossOriginAttributeValue new_crossorigin_state =
        GetCrossOriginAttributeValue(params.new_value);
    CrossOriginAttributeValue old_crossorigin_state =
        GetCrossOriginAttributeValue(params.old_value);

    if (new_crossorigin_state != old_crossorigin_state) {
      // Update the current state so we can detect future state changes.
      GetImageLoader().UpdateFromElement(
          ImageLoader::kUpdateIgnorePreviousError);
    }
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
  if (GetLayoutObject()) {
    GetImageLoader().OnAttachLayoutTree();
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

SVGAnimatedPropertyBase* SVGImageElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kXAttr) {
    return x_.Get();
  } else if (attribute_name == svg_names::kYAttr) {
    return y_.Get();
  } else if (attribute_name == svg_names::kWidthAttr) {
    return width_.Get();
  } else if (attribute_name == svg_names::kHeightAttr) {
    return height_.Get();
  } else if (attribute_name == svg_names::kPreserveAspectRatioAttr) {
    return preserve_aspect_ratio_.Get();
  } else {
    SVGAnimatedPropertyBase* ret =
        SVGURIReference::PropertyFromAttribute(attribute_name);
    if (ret) {
      return ret;
    } else {
      return SVGGraphicsElement::PropertyFromAttribute(attribute_name);
    }
  }
}

void SVGImageElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{x_.Get(), y_.Get(), width_.Get(),
                                   height_.Get(), preserve_aspect_ratio_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGURIReference::SynchronizeAllSVGAttributes();
  SVGGraphicsElement::SynchronizeAllSVGAttributes();
}

void SVGImageElement::CollectExtraStyleForPresentationAttribute(
    MutableCSSPropertyValueSet* style) {
  auto pres_attrs = std::to_array<const SVGAnimatedPropertyBase*>(
      {x_.Get(), y_.Get(), width_.Get(), height_.Get()});
  AddAnimatedPropertiesToPresentationAttributeStyle(pres_attrs, style);
  SVGGraphicsElement::CollectExtraStyleForPresentationAttribute(style);
}

}  // namespace blink
