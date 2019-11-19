/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_crossfade_value.h"

#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/graphics/crossfade_generated_image.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

static bool SubimageIsPending(const CSSValue& value) {
  if (auto* image_value = DynamicTo<CSSImageValue>(value))
    return image_value->IsCachePending();

  if (auto* image_generator_value = DynamicTo<CSSImageGeneratorValue>(value))
    return image_generator_value->IsPending();

  NOTREACHED();

  return false;
}

static bool SubimageKnownToBeOpaque(const CSSValue& value,
                                    const Document& document,
                                    const ComputedStyle& style) {
  if (auto* image_value = DynamicTo<CSSImageValue>(value))
    return image_value->KnownToBeOpaque(document, style);

  if (auto* img_generator_value = DynamicTo<CSSImageGeneratorValue>(value))
    return img_generator_value->KnownToBeOpaque(document, style);

  NOTREACHED();

  return false;
}

static ImageResourceContent* CachedImageForCSSValue(CSSValue* value,
                                                    const Document& document) {
  if (!value)
    return nullptr;

  if (auto* image_value = DynamicTo<CSSImageValue>(value)) {
    StyleImage* style_image_resource =
        image_value->CacheImage(document, FetchParameters::kAllowPlaceholder);
    if (!style_image_resource)
      return nullptr;

    return style_image_resource->CachedImage();
  }

  if (auto* img_generator_value = DynamicTo<CSSImageGeneratorValue>(value)) {
    img_generator_value->LoadSubimages(document);
    // FIXME: Handle CSSImageGeneratorValue (and thus cross-fades with gradients
    // and canvas).
    return nullptr;
  }

  NOTREACHED();

  return nullptr;
}

static Image* RenderableImageForCSSValue(CSSValue* value,
                                         const Document& document) {
  ImageResourceContent* cached_image = CachedImageForCSSValue(value, document);

  if (!cached_image || cached_image->ErrorOccurred() ||
      cached_image->GetImage()->IsNull())
    return nullptr;

  return cached_image->GetImage();
}

static KURL UrlForCSSValue(const CSSValue& value) {
  auto* image_value = DynamicTo<CSSImageValue>(value);
  if (!image_value)
    return KURL();

  return KURL(image_value->Url());
}

CSSCrossfadeValue::CSSCrossfadeValue(CSSValue* from_value,
                                     CSSValue* to_value,
                                     CSSPrimitiveValue* percentage_value)
    : CSSImageGeneratorValue(kCrossfadeClass),
      from_value_(from_value),
      to_value_(to_value),
      percentage_value_(percentage_value),
      cached_from_image_(nullptr),
      cached_to_image_(nullptr),
      crossfade_subimage_observer_(this) {}

CSSCrossfadeValue::~CSSCrossfadeValue() = default;

void CSSCrossfadeValue::Dispose() {
  if (cached_from_image_) {
    cached_from_image_->RemoveObserver(&crossfade_subimage_observer_);
    cached_from_image_ = nullptr;
  }
  if (cached_to_image_) {
    cached_to_image_->RemoveObserver(&crossfade_subimage_observer_);
    cached_to_image_ = nullptr;
  }
}

String CSSCrossfadeValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("-webkit-cross-fade(");
  result.Append(from_value_->CssText());
  result.Append(", ");
  result.Append(to_value_->CssText());
  result.Append(", ");
  result.Append(percentage_value_->CssText());
  result.Append(')');
  return result.ToString();
}

CSSCrossfadeValue* CSSCrossfadeValue::ComputedCSSValue(
    const ComputedStyle& style,
    bool allow_visited_style) {
  CSSValue* from_value = from_value_;
  if (auto* from_image_value = DynamicTo<CSSImageValue>(from_value_.Get())) {
    from_value = from_image_value->ValueWithURLMadeAbsolute();
  } else if (auto* from_generator_value =
                 DynamicTo<CSSImageGeneratorValue>(from_value_.Get())) {
    from_value =
        from_generator_value->ComputedCSSValue(style, allow_visited_style);
  }
  CSSValue* to_value = to_value_;
  if (auto* to_image_value = DynamicTo<CSSImageValue>(to_value_.Get())) {
    to_value = to_image_value->ValueWithURLMadeAbsolute();
  } else if (auto* to_generator_value =
                 DynamicTo<CSSImageGeneratorValue>(to_value_.Get())) {
    to_value = to_generator_value->ComputedCSSValue(style, allow_visited_style);
  }
  return MakeGarbageCollected<CSSCrossfadeValue>(from_value, to_value,
                                                 percentage_value_);
}

FloatSize CSSCrossfadeValue::FixedSize(
    const Document& document,
    const FloatSize& default_object_size) const {
  Image* from_image = RenderableImageForCSSValue(from_value_.Get(), document);
  Image* to_image = RenderableImageForCSSValue(to_value_.Get(), document);

  if (!from_image || !to_image)
    return FloatSize();

  FloatSize from_image_size(from_image->Size());
  FloatSize to_image_size(to_image->Size());

  if (from_image->IsSVGImage()) {
    from_image_size =
        ToSVGImage(from_image)->ConcreteObjectSize(default_object_size);
  }

  if (to_image->IsSVGImage()) {
    to_image_size =
        ToSVGImage(to_image)->ConcreteObjectSize(default_object_size);
  }

  // Rounding issues can cause transitions between images of equal size to
  // return a different fixed size; avoid performing the interpolation if the
  // images are the same size.
  if (from_image_size == to_image_size)
    return from_image_size;

  float percentage = percentage_value_->GetFloatValue();
  float inverse_percentage = 1 - percentage;

  return FloatSize(from_image_size.Width() * inverse_percentage +
                       to_image_size.Width() * percentage,
                   from_image_size.Height() * inverse_percentage +
                       to_image_size.Height() * percentage);
}

bool CSSCrossfadeValue::IsPending() const {
  return SubimageIsPending(*from_value_) || SubimageIsPending(*to_value_);
}

bool CSSCrossfadeValue::KnownToBeOpaque(const Document& document,
                                        const ComputedStyle& style) const {
  return SubimageKnownToBeOpaque(*from_value_, document, style) &&
         SubimageKnownToBeOpaque(*to_value_, document, style);
}

void CSSCrossfadeValue::LoadSubimages(const Document& document) {
  ImageResourceContent* old_cached_from_image = cached_from_image_;
  ImageResourceContent* old_cached_to_image = cached_to_image_;

  cached_from_image_ = CachedImageForCSSValue(from_value_.Get(), document);
  cached_to_image_ = CachedImageForCSSValue(to_value_.Get(), document);

  if (cached_from_image_ != old_cached_from_image) {
    if (old_cached_from_image)
      old_cached_from_image->RemoveObserver(&crossfade_subimage_observer_);
    if (cached_from_image_)
      cached_from_image_->AddObserver(&crossfade_subimage_observer_);
  }

  if (cached_to_image_ != old_cached_to_image) {
    if (old_cached_to_image)
      old_cached_to_image->RemoveObserver(&crossfade_subimage_observer_);
    if (cached_to_image_)
      cached_to_image_->AddObserver(&crossfade_subimage_observer_);
  }

  crossfade_subimage_observer_.SetReady(true);
}

scoped_refptr<Image> CSSCrossfadeValue::GetImage(
    const ImageResourceObserver& client,
    const Document& document,
    const ComputedStyle&,
    const FloatSize& size) const {
  if (size.IsEmpty())
    return nullptr;

  Image* from_image = RenderableImageForCSSValue(from_value_.Get(), document);
  Image* to_image = RenderableImageForCSSValue(to_value_.Get(), document);

  if (!from_image || !to_image)
    return Image::NullImage();

  scoped_refptr<Image> from_image_ref(from_image);
  scoped_refptr<Image> to_image_ref(to_image);

  if (from_image->IsSVGImage()) {
    from_image_ref = SVGImageForContainer::Create(
        ToSVGImage(from_image), size, 1, UrlForCSSValue(*from_value_));
  }

  if (to_image->IsSVGImage()) {
    to_image_ref = SVGImageForContainer::Create(ToSVGImage(to_image), size, 1,
                                                UrlForCSSValue(*to_value_));
  }

  return CrossfadeGeneratedImage::Create(from_image_ref, to_image_ref,
                                         percentage_value_->GetFloatValue(),
                                         FixedSize(document, size), size);
}

void CSSCrossfadeValue::CrossfadeChanged(
    ImageResourceObserver::CanDeferInvalidation defer) {
  for (const auto& curr : Clients()) {
    ImageResourceObserver* client =
        const_cast<ImageResourceObserver*>(curr.key);
    client->ImageChanged(static_cast<WrappedImagePtr>(this), defer);
  }
}

bool CSSCrossfadeValue::WillRenderImage() const {
  for (const auto& curr : Clients()) {
    if (const_cast<ImageResourceObserver*>(curr.key)->WillRenderImage())
      return true;
  }
  return false;
}

void CSSCrossfadeValue::CrossfadeSubimageObserverProxy::ImageChanged(
    ImageResourceContent*,
    CanDeferInvalidation defer) {
  if (ready_)
    owner_value_->CrossfadeChanged(defer);
}

bool CSSCrossfadeValue::CrossfadeSubimageObserverProxy::WillRenderImage() {
  // If the images are not ready/loaded we won't paint them. If the images
  // are ready then ask the clients.
  return ready_ && owner_value_->WillRenderImage();
}

bool CSSCrossfadeValue::HasFailedOrCanceledSubresources() const {
  if (cached_from_image_ && cached_from_image_->LoadFailedOrCanceled())
    return true;
  if (cached_to_image_ && cached_to_image_->LoadFailedOrCanceled())
    return true;
  return false;
}

bool CSSCrossfadeValue::Equals(const CSSCrossfadeValue& other) const {
  return DataEquivalent(from_value_, other.from_value_) &&
         DataEquivalent(to_value_, other.to_value_) &&
         DataEquivalent(percentage_value_, other.percentage_value_);
}

void CSSCrossfadeValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(from_value_);
  visitor->Trace(to_value_);
  visitor->Trace(percentage_value_);
  visitor->Trace(cached_from_image_);
  visitor->Trace(cached_to_image_);
  visitor->Trace(crossfade_subimage_observer_);
  CSSImageGeneratorValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
