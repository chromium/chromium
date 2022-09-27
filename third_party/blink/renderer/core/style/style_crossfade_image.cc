// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_crossfade_image.h"

#include "third_party/blink/renderer/core/css/css_crossfade_value.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/crossfade_generated_image.h"

namespace blink {

StyleCrossfadeImage::StyleCrossfadeImage(cssvalue::CSSCrossfadeValue& value,
                                         StyleImage* from_image,
                                         StyleImage* to_image)
    : original_value_(value), from_image_(from_image), to_image_(to_image) {
  is_crossfade_ = true;
}

StyleCrossfadeImage::~StyleCrossfadeImage() = default;

bool StyleCrossfadeImage::IsEqual(const StyleImage& other) const {
  if (!other.IsCrossfadeImage())
    return false;
  return original_value_ == To<StyleCrossfadeImage>(other).original_value_;
}

CSSValue* StyleCrossfadeImage::CssValue() const {
  return original_value_;
}

CSSValue* StyleCrossfadeImage::ComputedCSSValue(
    const ComputedStyle& style,
    bool allow_visited_style) const {
  // If either of the images are null (meaning that they are 'none'), then use
  // the original value.
  CSSValue* from_value =
      from_image_ ? from_image_->ComputedCSSValue(style, allow_visited_style)
                  : &original_value_->From();
  CSSValue* to_value =
      to_image_ ? to_image_->ComputedCSSValue(style, allow_visited_style)
                : &original_value_->To();
  return MakeGarbageCollected<cssvalue::CSSCrossfadeValue>(
      from_value, to_value, &original_value_->Percentage());
}

bool StyleCrossfadeImage::CanRender() const {
  return (!from_image_ || from_image_->CanRender()) &&
         (!to_image_ || to_image_->CanRender());
}

bool StyleCrossfadeImage::IsLoaded() const {
  return (!from_image_ || from_image_->IsLoaded()) &&
         (!to_image_ || to_image_->IsLoaded());
}

bool StyleCrossfadeImage::ErrorOccurred() const {
  return (from_image_ && from_image_->ErrorOccurred()) ||
         (to_image_ && to_image_->ErrorOccurred());
}

bool StyleCrossfadeImage::IsAccessAllowed(String& failing_url) const {
  return (!from_image_ || from_image_->IsAccessAllowed(failing_url)) &&
         (!to_image_ || to_image_->IsAccessAllowed(failing_url));
}

gfx::SizeF StyleCrossfadeImage::ImageSize(float multiplier,
                                          const gfx::SizeF& default_object_size,
                                          RespectImageOrientationEnum) const {
  if (!from_image_ || !to_image_)
    return gfx::SizeF();

  // TODO(fs): Consider |respect_orientation|?
  gfx::SizeF from_image_size = from_image_->ImageSize(
      multiplier, default_object_size, kRespectImageOrientation);
  gfx::SizeF to_image_size = to_image_->ImageSize(
      multiplier, default_object_size, kRespectImageOrientation);

  // Rounding issues can cause transitions between images of equal size to
  // return a different fixed size; avoid performing the interpolation if the
  // images are the same size.
  if (from_image_size == to_image_size)
    return from_image_size;

  float percentage = original_value_->Percentage().GetFloatValue();
  float inverse_percentage = 1 - percentage;
  return gfx::SizeF(from_image_size.width() * inverse_percentage +
                        to_image_size.width() * percentage,
                    from_image_size.height() * inverse_percentage +
                        to_image_size.height() * percentage);
}

bool StyleCrossfadeImage::HasIntrinsicSize() const {
  return (from_image_ && from_image_->HasIntrinsicSize()) ||
         (to_image_ && to_image_->HasIntrinsicSize());
}

void StyleCrossfadeImage::AddClient(ImageResourceObserver* observer) {
  const bool had_clients = original_value_->HasClients();
  original_value_->AddClient(observer);
  if (had_clients)
    return;
  ImageResourceObserver* proxy_observer = original_value_->GetObserverProxy();
  if (from_image_)
    from_image_->AddClient(proxy_observer);
  if (to_image_)
    to_image_->AddClient(proxy_observer);
}

void StyleCrossfadeImage::RemoveClient(ImageResourceObserver* observer) {
  original_value_->RemoveClient(observer);
  if (original_value_->HasClients())
    return;
  ImageResourceObserver* proxy_observer = original_value_->GetObserverProxy();
  if (from_image_)
    from_image_->RemoveClient(proxy_observer);
  if (to_image_)
    to_image_->RemoveClient(proxy_observer);
}

scoped_refptr<Image> StyleCrossfadeImage::GetImage(
    const ImageResourceObserver& observer,
    const Document& document,
    const ComputedStyle& style,
    const gfx::SizeF& target_size) const {
  if (target_size.IsEmpty())
    return nullptr;
  if (!from_image_ || !to_image_)
    return Image::NullImage();
  const gfx::SizeF resolved_size =
      ImageSize(style.EffectiveZoom(), target_size, kRespectImageOrientation);
  const ImageResourceObserver* proxy_observer =
      original_value_->GetObserverProxy();
  return CrossfadeGeneratedImage::Create(
      from_image_->GetImage(*proxy_observer, document, style, target_size),
      to_image_->GetImage(*proxy_observer, document, style, target_size),
      original_value_->Percentage().GetFloatValue(), resolved_size);
}

WrappedImagePtr StyleCrossfadeImage::Data() const {
  return original_value_.Get();
}

bool StyleCrossfadeImage::KnownToBeOpaque(const Document& document,
                                          const ComputedStyle& style) const {
  return from_image_ && from_image_->KnownToBeOpaque(document, style) &&
         to_image_ && to_image_->KnownToBeOpaque(document, style);
}

void StyleCrossfadeImage::Trace(Visitor* visitor) const {
  visitor->Trace(original_value_);
  visitor->Trace(from_image_);
  visitor->Trace(to_image_);
  StyleImage::Trace(visitor);
}

}  // namespace blink
