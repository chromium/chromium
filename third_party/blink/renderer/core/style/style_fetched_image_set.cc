/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/style/style_fetched_image_set.h"

#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/graphics/placeholder_image.h"

namespace blink {

StyleFetchedImageSet::StyleFetchedImageSet(ImageResourceContent* image,
                                           float image_scale_factor,
                                           CSSImageSetValue* value,
                                           const KURL& url)
    : best_fit_image_(image),
      image_scale_factor_(image_scale_factor),
      image_set_value_(value),
      url_(url) {
  is_image_resource_set_ = true;
  best_fit_image_->AddObserver(this);
}

StyleFetchedImageSet::~StyleFetchedImageSet() = default;

void StyleFetchedImageSet::Dispose() {
  best_fit_image_->RemoveObserver(this);
  best_fit_image_ = nullptr;
}

bool StyleFetchedImageSet::IsEqual(const StyleImage& other) const {
  if (!other.IsImageResourceSet())
    return false;
  const auto& other_image = To<StyleFetchedImageSet>(other);
  if (best_fit_image_ != other_image.best_fit_image_)
    return false;
  return url_ == other_image.url_;
}

WrappedImagePtr StyleFetchedImageSet::Data() const {
  return best_fit_image_.Get();
}

ImageResourceContent* StyleFetchedImageSet::CachedImage() const {
  return best_fit_image_.Get();
}

CSSValue* StyleFetchedImageSet::CssValue() const {
  return image_set_value_;
}

CSSValue* StyleFetchedImageSet::ComputedCSSValue(
    const ComputedStyle& style,
    bool allow_visited_style) const {
  return image_set_value_->ValueWithURLsMadeAbsolute();
}

bool StyleFetchedImageSet::CanRender() const {
  return !best_fit_image_->ErrorOccurred() &&
         !best_fit_image_->GetImage()->IsNull();
}

bool StyleFetchedImageSet::IsLoaded() const {
  return best_fit_image_->IsLoaded();
}

bool StyleFetchedImageSet::ErrorOccurred() const {
  return best_fit_image_->ErrorOccurred();
}

FloatSize StyleFetchedImageSet::ImageSize(
    const Document&,
    float multiplier,
    const LayoutSize& default_object_size) const {
  Image* image = best_fit_image_->GetImage();
  if (image->IsSVGImage()) {
    return ImageSizeForSVGImage(ToSVGImage(image), multiplier,
                                default_object_size);
  }
  // Image orientation should only be respected for content images,
  // not decorative ones such as StyleImage (backgrounds,
  // border-image, etc.)
  //
  // https://drafts.csswg.org/css-images-3/#the-image-orientation
  FloatSize natural_size(image->Size());
  FloatSize scaled_image_size(ApplyZoom(natural_size, multiplier));
  scaled_image_size.Scale(1 / image_scale_factor_);
  return scaled_image_size;
}

bool StyleFetchedImageSet::HasIntrinsicSize() const {
  return best_fit_image_->GetImage()->HasIntrinsicSize();
}

void StyleFetchedImageSet::AddClient(ImageResourceObserver* observer) {
  best_fit_image_->AddObserver(observer);
}

void StyleFetchedImageSet::RemoveClient(ImageResourceObserver* observer) {
  best_fit_image_->RemoveObserver(observer);
}

scoped_refptr<Image> StyleFetchedImageSet::GetImage(
    const ImageResourceObserver&,
    const Document&,
    const ComputedStyle& style,
    const FloatSize& target_size) const {
  Image* image = best_fit_image_->GetImage();
  if (image->IsPlaceholderImage()) {
    static_cast<PlaceholderImage*>(image)->SetIconAndTextScaleFactor(
        style.EffectiveZoom());
  }

  if (!image->IsSVGImage())
    return image;
  return SVGImageForContainer::Create(ToSVGImage(image), target_size,
                                      style.EffectiveZoom(), url_);
}

bool StyleFetchedImageSet::KnownToBeOpaque(const Document&,
                                           const ComputedStyle&) const {
  return best_fit_image_->GetImage()->CurrentFrameKnownToBeOpaque();
}

void StyleFetchedImageSet::Trace(blink::Visitor* visitor) {
  visitor->Trace(best_fit_image_);
  visitor->Trace(image_set_value_);
  StyleImage::Trace(visitor);
}

}  // namespace blink
