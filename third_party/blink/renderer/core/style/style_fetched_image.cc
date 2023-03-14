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
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/placeholder_image.h"

namespace blink {

StyleFetchedImage::StyleFetchedImage(ImageResourceContent* image,
                                     const Document& document,
                                     bool is_lazyload_possibly_deferred,
                                     bool origin_clean,
                                     bool is_ad_related,
                                     const KURL& url,
                                     const float override_image_resolution)
    : document_(document),
      url_(url),
      override_image_resolution_(override_image_resolution),
      origin_clean_(origin_clean),
      is_ad_related_(is_ad_related) {
  is_image_resource_ = true;
  is_lazyload_possibly_deferred_ = is_lazyload_possibly_deferred;

  const PaintTiming* paint_timing = PaintTiming::From(document);
  is_loaded_after_mouseover_ =
      paint_timing && paint_timing->IsLCPMouseoverDispatchedRecently();

  image_ = image;
  image_->AddObserver(this);
  // ResourceFetcher is not determined from StyleFetchedImage and it is
  // impossible to send a request for refetching.
  image_->SetNotRefetchableDataFromDiskCache();
}

StyleFetchedImage::~StyleFetchedImage() = default;

void StyleFetchedImage::Prefinalize() {
  image_->DidRemoveObserver();
  image_ = nullptr;
}

bool StyleFetchedImage::IsEqual(const StyleImage& other) const {
  if (!other.IsImageResource()) {
    return false;
  }

  const auto& other_image = To<StyleFetchedImage>(other);

  return image_ == other_image.image_ && url_ == other_image.url_ &&
         EqualResolutions(override_image_resolution_,
                          other_image.override_image_resolution_);
}

WrappedImagePtr StyleFetchedImage::Data() const {
  return image_.Get();
}

float StyleFetchedImage::ImageScaleFactor() const {
  if (override_image_resolution_ > 0.0f) {
    return override_image_resolution_;
  }

  if (image_->HasDevicePixelRatioHeaderValue()) {
    return image_->DevicePixelRatioHeaderValue();
  }

  return 1.0f;
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

bool StyleFetchedImage::IsLoading() const {
  return image_->IsLoading();
}

bool StyleFetchedImage::ErrorOccurred() const {
  return image_->ErrorOccurred();
}

bool StyleFetchedImage::IsAccessAllowed(String& failing_url) const {
  DCHECK(image_->IsLoaded());
  if (image_->IsAccessAllowed()) {
    return true;
  }
  failing_url = image_->Url().ElidedString();
  return false;
}

gfx::SizeF StyleFetchedImage::ImageSize(
    float multiplier,
    const gfx::SizeF& default_object_size,
    RespectImageOrientationEnum respect_orientation) const {
  Image* image = image_->GetImage();

  if (image->IsBitmapImage() && override_image_resolution_ > 0.0f) {
    multiplier /= override_image_resolution_;
  } else if (image_->HasDevicePixelRatioHeaderValue()) {
    multiplier /= image_->DevicePixelRatioHeaderValue();
  }

  if (auto* svg_image = DynamicTo<SVGImage>(image)) {
    return ImageSizeForSVGImage(*svg_image, multiplier, default_object_size);
  }

  respect_orientation = ForceOrientationIfNecessary(respect_orientation);
  gfx::SizeF size(image->Size(respect_orientation));

  return ApplyZoom(size, multiplier);
}

bool StyleFetchedImage::HasIntrinsicSize() const {
  const Image& image = *image_->GetImage();
  if (auto* svg_image = DynamicTo<SVGImage>(image)) {
    return HasIntrinsicDimensionsForSVGImage(*svg_image);
  }
  return image.HasIntrinsicSize();
}

void StyleFetchedImage::AddClient(ImageResourceObserver* observer) {
  image_->AddObserver(observer);
}

void StyleFetchedImage::RemoveClient(ImageResourceObserver* observer) {
  image_->RemoveObserver(observer);
}

void StyleFetchedImage::ImageNotifyFinished(ImageResourceContent*) {
  if (!document_) {
    return;
  }

  if (image_ && image_->HasImage()) {
    Image& image = *image_->GetImage();

    if (auto* svg_image = DynamicTo<SVGImage>(image)) {
      // SVG's document should be completely loaded before access control
      // checks, which can occur anytime after ImageNotifyFinished()
      // (See SVGImage::CurrentFrameHasSingleSecurityOrigin()).
      // We check the document is loaded here to catch violation of the
      // assumption reliably.
      svg_image->CheckLoaded();
      svg_image->UpdateUseCounters(*document_);
    }
    image_->RecordDecodedImageType(document_->GetExecutionContext());
  }

  if (LocalDOMWindow* window = document_->domWindow()) {
    ImageElementTiming::From(*window).NotifyBackgroundImageFinished(this);
  }

  // Oilpan: do not prolong the Document's lifetime.
  document_.Clear();
}

scoped_refptr<Image> StyleFetchedImage::GetImage(
    const ImageResourceObserver&,
    const Document& document,
    const ComputedStyle& style,
    const gfx::SizeF& target_size) const {
  Image* image = image_->GetImage();
  if (image->IsPlaceholderImage()) {
    static_cast<PlaceholderImage*>(image)->SetIconAndTextScaleFactor(
        style.EffectiveZoom());
  }

  auto* svg_image = DynamicTo<SVGImage>(image);
  if (!svg_image) {
    return image;
  }
  return SVGImageForContainer::Create(
      svg_image, target_size, style.EffectiveZoom(), url_,
      document.GetStyleEngine().ResolveColorSchemeForEmbedding(&style));
}

bool StyleFetchedImage::KnownToBeOpaque(const Document&,
                                        const ComputedStyle&) const {
  return image_->GetImage()->CurrentFrameKnownToBeOpaque();
}

void StyleFetchedImage::LoadDeferredImage(const Document& document) {
  DCHECK(is_lazyload_possibly_deferred_);
  is_lazyload_possibly_deferred_ = false;
  document_ = &document;
  image_->LoadDeferredImage(document_->Fetcher());
}

RespectImageOrientationEnum StyleFetchedImage::ForceOrientationIfNecessary(
    RespectImageOrientationEnum default_orientation) const {
  // SVG Images don't have orientation and assert on loading when
  // IsAccessAllowed is called.
  if (image_->GetImage()->IsSVGImage()) {
    return default_orientation;
  }
  // Cross-origin images must always respect orientation to prevent
  // potentially private data leakage.
  if (!image_->IsAccessAllowed()) {
    return kRespectImageOrientation;
  }
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
  ImageResourceObserver::Trace(visitor);
}

}  // namespace blink
