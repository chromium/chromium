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

#include "third_party/blink/renderer/core/style/style_image_set.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/css_image_set_option_value.h"
#include "third_party/blink/renderer/core/style/style_image_computed_css_value_builder.h"

namespace blink {

StyleImageSet::StyleImageSet(StyleImage* best_fit_image,
                             CSSImageSetValue* image_set_value,
                             bool is_origin_clean)
    : best_fit_image_(best_fit_image),
      image_set_value_(image_set_value),
      is_origin_clean_(is_origin_clean) {
  is_image_resource_set_ = true;
}

StyleImageSet::~StyleImageSet() = default;

bool StyleImageSet::IsEqual(const StyleImage& other) const {
  const auto* other_image_set = DynamicTo<StyleImageSet>(other);

  return other_image_set &&
         base::ValuesEquivalent(best_fit_image_,
                                other_image_set->best_fit_image_) &&
         image_set_value_->Equals(*other_image_set->image_set_value_);
}

WrappedImagePtr StyleImageSet::Data() const {
  return best_fit_image_ ? best_fit_image_->Data() : nullptr;
}

ImageResourceContent* StyleImageSet::CachedImage() const {
  return best_fit_image_ ? best_fit_image_->CachedImage() : nullptr;
}

CSSValue* StyleImageSet::CssValue() const {
  return image_set_value_.Get();
}

CSSValue* StyleImageSet::ComputedCSSValue(const ComputedStyle& style,
                                          bool allow_visited_style,
                                          CSSValuePhase value_phase) const {
  return StyleImageComputedCSSValueBuilder(style, allow_visited_style,
                                           value_phase)
      .Build(image_set_value_);
}

bool StyleImageSet::CanRender() const {
  return best_fit_image_ && best_fit_image_->CanRender();
}

bool StyleImageSet::IsLoaded() const {
  return !best_fit_image_ || best_fit_image_->IsLoaded();
}

bool StyleImageSet::IsLoading() const {
  return best_fit_image_ && best_fit_image_->IsLoading();
}

bool StyleImageSet::ErrorOccurred() const {
  return best_fit_image_ && best_fit_image_->ErrorOccurred();
}

bool StyleImageSet::IsAccessAllowed(String& failing_url) const {
  return !best_fit_image_ || best_fit_image_->IsAccessAllowed(failing_url);
}

IntrinsicSizingInfo StyleImageSet::GetNaturalSizingInfo(
    float multiplier,
    RespectImageOrientationEnum respect_orientation) const {
  if (best_fit_image_) {
    return best_fit_image_->GetNaturalSizingInfo(multiplier,
                                                 respect_orientation);
  }
  return IntrinsicSizingInfo::None();
}

gfx::SizeF StyleImageSet::ImageSize(
    float multiplier,
    const gfx::SizeF& default_object_size,
    RespectImageOrientationEnum respect_orientation) const {
  return best_fit_image_
             ? best_fit_image_->ImageSize(multiplier, default_object_size,
                                          respect_orientation)
             : gfx::SizeF();
}

bool StyleImageSet::HasIntrinsicSize() const {
  return best_fit_image_ && best_fit_image_->HasIntrinsicSize();
}

void StyleImageSet::AddClient(ImageResourceObserver* observer) {
  if (!best_fit_image_) {
    return;
  }

  best_fit_image_->AddClient(observer);
}

void StyleImageSet::RemoveClient(ImageResourceObserver* observer) {
  if (!best_fit_image_) {
    return;
  }

  best_fit_image_->RemoveClient(observer);
}

scoped_refptr<Image> StyleImageSet::GetImage(
    const ImageResourceObserver& image_resource_observer,
    const Document& document,
    const ComputedStyle& style,
    const gfx::SizeF& target_size) const {
  return best_fit_image_
             ? best_fit_image_->GetImage(image_resource_observer, document,
                                         style, target_size)
             : nullptr;
}

float StyleImageSet::ImageScaleFactor() const {
  return best_fit_image_ ? best_fit_image_->ImageScaleFactor() : 0.0f;
}

bool StyleImageSet::KnownToBeOpaque(const Document& document,
                                    const ComputedStyle& computed_style) const {
  return best_fit_image_ &&
         best_fit_image_->KnownToBeOpaque(document, computed_style);
}

RespectImageOrientationEnum StyleImageSet::ForceOrientationIfNecessary(
    RespectImageOrientationEnum default_orientation) const {
  return best_fit_image_
             ? best_fit_image_->ForceOrientationIfNecessary(default_orientation)
             : RespectImageOrientationEnum::kDoNotRespectImageOrientation;
}

void StyleImageSet::Trace(Visitor* visitor) const {
  visitor->Trace(best_fit_image_);
  visitor->Trace(image_set_value_);
  StyleImage::Trace(visitor);
}

}  // namespace blink
