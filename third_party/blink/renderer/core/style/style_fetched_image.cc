/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/style/style_fetched_image.h"

#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/placeholder_image.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

StyleFetchedImage::StyleFetchedImage(const Document& document,
                                     FetchParameters& params,
                                     bool is_lazyload_possibly_deferred)
    : document_(&document),
      url_(params.Url()),
      origin_clean_(!params.IsFromOriginDirtyStyleSheet()),
      is_ad_related_(params.GetResourceRequest().IsAdResource()) {
  is_image_resource_ = true;
  is_lazyload_possibly_deferred_ = is_lazyload_possibly_deferred;

  image_ = ImageResourceContent::Fetch(params, document_->Fetcher());
  image_->AddObserver(this);
  // ResourceFetcher is not determined from StyleFetchedImage and it is
  // impossible to send a request for refetching.
  image_->SetNotRefetchableDataFromDiskCache();
}

StyleFetchedImage::~StyleFetchedImage() = default;

void StyleFetchedImage::Dispose() {
  image_->RemoveObserver(this);
  image_ = nullptr;
}

bool StyleFetchedImage::IsEqual(const StyleImage& other) const {
  if (!other.IsImageResource())
    return false;
  const auto& other_image = To<StyleFetchedImage>(other);
  if (image_ != other_image.image_)
    return false;
  return url_ == other_image.url_;
}

WrappedImagePtr StyleFetchedImage::Data() const {
  return image_.Get();
}

ImageResourceContent* StyleFetchedImage::CachedImage() const {
  return image_.Get();
}

CSSValue* StyleFetchedImage::CssValue() const {
  return MakeGarbageCollected<CSSImageValue>(
      AtomicString(url_.GetString()), url_, Referrer(),
      origin_clean_ ? OriginClean::kTrue : OriginClean::kFalse, is_ad_related_,
      const_cast<StyleFetchedImage*>(this));
}

CSSValue* StyleFetchedImage::ComputedCSSValue(const ComputedStyle&,
                                              bool allow_visited_style) const {
  return CssValue();
}

bool StyleFetchedImage::CanRender() const {
  return !image_->ErrorOccurred() && !image_->GetImage()->IsNull();
}

bool StyleFetchedImage::IsLoaded() const {
  return image_->IsLoaded();
}

bool StyleFetchedImage::ErrorOccurred() const {
  return image_->ErrorOccurred();
}

FloatSize StyleFetchedImage::ImageSize(
    const Document&,
    float multiplier,
    const FloatSize& default_object_size,
    RespectImageOrientationEnum respect_orientation) const {
  Image* image = image_->GetImage();
  if (image_->HasDevicePixelRatioHeaderValue()) {
    multiplier /= image_->DevicePixelRatioHeaderValue();
  }
  if (auto* svg_image = DynamicTo<SVGImage>(image)) {
    return ImageSizeForSVGImage(svg_image, multiplier, default_object_size);
  }
  respect_orientation = ForceOrientationIfNecessary(respect_orientation);
  FloatSize size(image->Size(respect_orientation));
  return ApplyZoom(size, multiplier);
}

bool StyleFetchedImage::HasIntrinsicSize() const {
  return image_->GetImage()->HasIntrinsicSize();
}

void StyleFetchedImage::AddClient(ImageResourceObserver* observer) {
  image_->AddObserver(observer);
}

void StyleFetchedImage::RemoveClient(ImageResourceObserver* observer) {
  image_->RemoveObserver(observer);
}

void StyleFetchedImage::ImageNotifyFinished(ImageResourceContent*) {
  if (image_ && image_->HasImage()) {
    Image& image = *image_->GetImage();

    auto* svg_image = DynamicTo<SVGImage>(image);
    if (document_ && svg_image)
      svg_image->UpdateUseCounters(*document_);
  }

  if (document_) {
    if (LocalDOMWindow* window = document_->domWindow())
      ImageElementTiming::From(*window).NotifyBackgroundImageFinished(this);
  }

  // Oilpan: do not prolong the Document's lifetime.
  document_.Clear();
}

scoped_refptr<Image> StyleFetchedImage::GetImage(
    const ImageResourceObserver&,
    const Document&,
    const ComputedStyle& style,
    const FloatSize& target_size) const {
  Image* image = image_->GetImage();
  if (image->IsPlaceholderImage()) {
    static_cast<PlaceholderImage*>(image)->SetIconAndTextScaleFactor(
        style.EffectiveZoom());
  }

  auto* svg_image = DynamicTo<SVGImage>(image);
  if (!svg_image)
    return image;
  return SVGImageForContainer::Create(svg_image, target_size,
                                      style.EffectiveZoom(), url_);
}

bool StyleFetchedImage::KnownToBeOpaque(const Document&,
                                        const ComputedStyle&) const {
  return image_->GetImage()->CurrentFrameKnownToBeOpaque();
}

void StyleFetchedImage::LoadDeferredImage(const Document& document) {
  DCHECK(is_lazyload_possibly_deferred_);
  is_lazyload_possibly_deferred_ = false;
  document_ = &document;
  if (document.GetFrame() && document.GetFrame()->Client()) {
    document.GetFrame()->Client()->DidObserveLazyLoadBehavior(
        WebLocalFrameClient::LazyLoadBehavior::kLazyLoadedImage);
  }
  image_->LoadDeferredImage(document_->Fetcher());
}

RespectImageOrientationEnum StyleFetchedImage::ForceOrientationIfNecessary(
    RespectImageOrientationEnum default_orientation) const {
  // SVG Images don't have orientation and assert on loading when
  // IsAccessAllowed is called.
  if (image_->GetImage()->IsSVGImage())
    return default_orientation;
  // Cross-origin images must always respect orientation to prevent
  // potentially private data leakage.
  if (!image_->IsAccessAllowed())
    return kRespectImageOrientation;
  return default_orientation;
}

bool StyleFetchedImage::GetImageAnimationPolicy(
    mojom::blink::ImageAnimationPolicy& policy) {
  if (!document_ || !document_->GetSettings()) {
    return false;
  }
  policy = document_->GetSettings()->GetImageAnimationPolicy();
  return true;
}

void StyleFetchedImage::Trace(Visitor* visitor) const {
  visitor->Trace(image_);
  visitor->Trace(document_);
  StyleImage::Trace(visitor);
}

}  // namespace blink
