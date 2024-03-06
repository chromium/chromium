// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_mask_source_image.h"

#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

StyleMaskSourceImage::StyleMaskSourceImage(StyleFetchedImage* image,
                                           SVGResource* resource,
                                           CSSImageValue* resource_css_value)
    : image_(image),
      resource_(resource),
      resource_css_value_(resource_css_value) {
  is_mask_source_ = true;
}

StyleMaskSourceImage::StyleMaskSourceImage(SVGResource* resource,
                                           CSSImageValue* resource_css_value)
    : StyleMaskSourceImage(nullptr, resource, resource_css_value) {}

StyleMaskSourceImage::~StyleMaskSourceImage() = default;

CSSValue* StyleMaskSourceImage::CssValue() const {
  return resource_css_value_.Get();
}

CSSValue* StyleMaskSourceImage::ComputedCSSValue(
    const ComputedStyle& style,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  return resource_css_value_->ComputedCSSValueMaybeLocal();
}

bool StyleMaskSourceImage::CanRender() const {
  return !image_ || image_->CanRender();
}

bool StyleMaskSourceImage::IsLoaded() const {
  return !image_ || image_->IsLoaded();
}

bool StyleMaskSourceImage::IsLoading() const {
  return image_ && image_->IsLoading();
}

bool StyleMaskSourceImage::ErrorOccurred() const {
  return image_ && image_->ErrorOccurred();
}

bool StyleMaskSourceImage::IsAccessAllowed(String& failing_url) const {
  return !image_ || image_->IsAccessAllowed(failing_url);
}

IntrinsicSizingInfo StyleMaskSourceImage::GetNaturalSizingInfo(
    float multiplier,
    RespectImageOrientationEnum respect_orientation) const {
  if (!image_) {
    return IntrinsicSizingInfo::None();
  }
  return image_->GetNaturalSizingInfo(multiplier, respect_orientation);
}

gfx::SizeF StyleMaskSourceImage::ImageSize(
    float multiplier,
    const gfx::SizeF& default_object_size,
    RespectImageOrientationEnum respect_orientation) const {
  if (!image_) {
    return gfx::SizeF();
  }
  return image_->ImageSize(multiplier, default_object_size,
                           respect_orientation);
}

bool StyleMaskSourceImage::HasIntrinsicSize() const {
  return image_ && image_->HasIntrinsicSize();
}

SVGResource* StyleMaskSourceImage::GetSVGResource() const {
  return resource_.Get();
}

SVGResourceClient* StyleMaskSourceImage::GetSVGResourceClient(
    const ImageResourceObserver& observer) const {
  return resource_ ? resource_->GetObserverResourceClient(
                         const_cast<ImageResourceObserver&>(observer))
                   : nullptr;
}

void StyleMaskSourceImage::AddClient(ImageResourceObserver* observer) {
  if (image_) {
    image_->AddClient(observer);
  }
  if (resource_) {
    resource_->AddObserver(*observer);
  }
}

void StyleMaskSourceImage::RemoveClient(ImageResourceObserver* observer) {
  if (image_) {
    image_->RemoveClient(observer);
  }
  if (resource_) {
    resource_->RemoveObserver(*observer);
  }
}

scoped_refptr<Image> StyleMaskSourceImage::GetImage(
    const ImageResourceObserver& observer,
    const Document& document,
    const ComputedStyle& style,
    const gfx::SizeF& target_size) const {
  if (!image_) {
    return Image::NullImage();
  }
  return image_->GetImage(observer, document, style, target_size);
}

float StyleMaskSourceImage::ImageScaleFactor() const {
  return image_ ? image_->ImageScaleFactor() : 1;
}

WrappedImagePtr StyleMaskSourceImage::Data() const {
  return image_ ? image_->Data() : resource_.Get();
}

bool StyleMaskSourceImage::KnownToBeOpaque(const Document& document,
                                           const ComputedStyle& style) const {
  return image_ && image_->KnownToBeOpaque(document, style);
}

ImageResourceContent* StyleMaskSourceImage::CachedImage() const {
  return image_ ? image_->CachedImage() : nullptr;
}

bool StyleMaskSourceImage::HasSVGMask() const {
  // If `image_` is null then this has to be an SVG <mask> reference.
  if (!image_) {
    return true;
  }
  CHECK(resource_);
  LayoutSVGResourceContainer* container =
      resource_->ResourceContainerNoCycleCheck();
  return IsA<LayoutSVGResourceMasker>(container);
}

bool StyleMaskSourceImage::IsEqual(const StyleImage& other) const {
  if (other.IsPendingImage()) {
    // Ignore pending status when comparing; as long as the values are
    // equal, the images should be considered equal, too.
    return base::ValuesEquivalent(CssValue(), other.CssValue());
  }
  const auto* other_mask_ref = DynamicTo<StyleMaskSourceImage>(other);
  return other_mask_ref &&
         base::ValuesEquivalent(image_, other_mask_ref->image_) &&
         resource_ == other_mask_ref->resource_;
}

void StyleMaskSourceImage::Trace(Visitor* visitor) const {
  visitor->Trace(image_);
  visitor->Trace(resource_);
  visitor->Trace(resource_css_value_);
  StyleImage::Trace(visitor);
}

}  // namespace blink
