// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_svg_mask_reference_image.h"

#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

StyleSVGMaskReferenceImage::StyleSVGMaskReferenceImage(
    SVGResource* resource,
    CSSImageValue* resource_css_value)
    : resource_(resource), resource_css_value_(resource_css_value) {
  is_svg_mask_reference_ = true;
}

StyleSVGMaskReferenceImage::~StyleSVGMaskReferenceImage() = default;

CSSValue* StyleSVGMaskReferenceImage::CssValue() const {
  return resource_css_value_.Get();
}

CSSValue* StyleSVGMaskReferenceImage::ComputedCSSValue(
    const ComputedStyle& style,
    bool allow_visited_style) const {
  return resource_css_value_->ComputedCSSValueMaybeLocal();
}

bool StyleSVGMaskReferenceImage::IsAccessAllowed(String& failing_url) const {
  return true;
}

IntrinsicSizingInfo StyleSVGMaskReferenceImage::GetNaturalSizingInfo(
    float multiplier,
    RespectImageOrientationEnum respect_orientation) const {
  return IntrinsicSizingInfo::None();
}

gfx::SizeF StyleSVGMaskReferenceImage::ImageSize(
    float multiplier,
    const gfx::SizeF& default_object_size,
    RespectImageOrientationEnum respect_orientation) const {
  return default_object_size;
}

bool StyleSVGMaskReferenceImage::HasIntrinsicSize() const {
  return false;
}

SVGResource* StyleSVGMaskReferenceImage::GetSVGResource() const {
  return resource_.Get();
}

SVGResourceClient* StyleSVGMaskReferenceImage::GetSVGResourceClient(
    const ImageResourceObserver& observer) const {
  return resource_ ? resource_->GetObserverResourceClient(
                         const_cast<ImageResourceObserver&>(observer))
                   : nullptr;
}

void StyleSVGMaskReferenceImage::AddClient(ImageResourceObserver* observer) {
  if (!resource_) {
    return;
  }
  resource_->AddObserver(*observer);
}

void StyleSVGMaskReferenceImage::RemoveClient(ImageResourceObserver* observer) {
  if (!resource_) {
    return;
  }
  resource_->RemoveObserver(*observer);
}

scoped_refptr<Image> StyleSVGMaskReferenceImage::GetImage(
    const ImageResourceObserver& observer,
    const Document& document,
    const ComputedStyle& style,
    const gfx::SizeF& target_size) const {
  return Image::NullImage();
}

WrappedImagePtr StyleSVGMaskReferenceImage::Data() const {
  return resource_.Get();
}

bool StyleSVGMaskReferenceImage::KnownToBeOpaque(
    const Document& document,
    const ComputedStyle& style) const {
  return false;
}

bool StyleSVGMaskReferenceImage::IsEqual(const StyleImage& other) const {
  if (other.IsPendingImage()) {
    // Ignore pending status when comparing; as long as the values are
    // equal, the same, the images should be considered equal, too.
    return base::ValuesEquivalent(CssValue(), other.CssValue());
  }
  const auto* other_mask_ref = DynamicTo<StyleSVGMaskReferenceImage>(other);
  return other_mask_ref && resource_ == other_mask_ref->resource_;
}

void StyleSVGMaskReferenceImage::Trace(Visitor* visitor) const {
  visitor->Trace(resource_);
  visitor->Trace(resource_css_value_);
  StyleImage::Trace(visitor);
}

}  // namespace blink
