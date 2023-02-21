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

#include "third_party/blink/renderer/core/css/css_image_set_value.h"

#include <algorithm>

#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/style/style_image_set.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSImageSetValue::CSSImageSetValue()
    : CSSValueList(kImageSetClass, kCommaSeparator) {}

CSSImageSetValue::~CSSImageSetValue() = default;

const CSSImageSetValue::ImageSetOption& CSSImageSetValue::GetBestOption(
    const float device_scale_factor) {
  // This method is implementing the selection logic described in the
  // "CSS Images Module Level 4" spec:
  // https://w3c.github.io/csswg-drafts/css-images-4/#image-set-notation
  //
  // Spec definition of image-set-option selection algorithm:
  //
  // "An image-set() function contains a list of one or more
  // <image-set-option>s, and must select only one of them
  // to determine what image it will represent:
  //
  //   1. First, remove any <image-set-option>s from the list that specify an
  //      unknown or unsupported MIME type in their type() value.
  //   2. Second, remove any <image-set-option>s from the list that have the
  //      same <resolution> as a previous option in the list.
  //   3. Finally, among the remaining <image-set-option>s, make a UA-specific
  //      choice of which to load, based on whatever criteria deemed relevant
  //      (such as the resolution of the display, connection speed, etc).
  //   4. The image-set() function then represents the <image> of the chosen
  //      <image-set-option>."

  if (options_.empty()) {
    for (wtf_size_t i = 0, length = this->length(); i < length; ++i) {
      auto image_index = i;

      ++i;
      SECURITY_DCHECK(i < length);
      float resolution = To<CSSPrimitiveValue>(Item(i)).ComputeDotsPerPixel();

      options_.push_back(ImageSetOption{image_index, resolution});
    }

    std::stable_sort(
        options_.begin(), options_.end(),
        [](const ImageSetOption& left, const ImageSetOption& right) {
          return left.resolution < right.resolution;
        });
  }

  for (const auto& image : options_) {
    if (image.resolution >= device_scale_factor) {
      return image;
    }
  }

  DCHECK(!options_.empty());

  return options_.back();
}

bool CSSImageSetValue::IsCachePending(const float device_scale_factor) const {
  return !cached_image_ ||
         !EqualResolutions(device_scale_factor, cached_device_scale_factor_);
}

StyleImage* CSSImageSetValue::CachedImage(
    const float device_scale_factor) const {
  DCHECK(!IsCachePending(device_scale_factor));
  return cached_image_.Get();
}

StyleImage* CSSImageSetValue::CacheImage(
    const Document& document,
    const float device_scale_factor,
    const FetchParameters::ImageRequestBehavior image_request_behavior,
    const CrossOriginAttributeValue cross_origin,
    const CSSToLengthConversionData::ContainerSizes& container_sizes) {
  if (IsCachePending(device_scale_factor)) {
    StyleImage* style_image =
        GetImageToCache(device_scale_factor, document, image_request_behavior,
                        cross_origin, container_sizes);

    cached_image_ = MakeGarbageCollected<StyleImageSet>(style_image, this);

    cached_device_scale_factor_ = device_scale_factor;
  }

  return cached_image_.Get();
}

StyleImage* CSSImageSetValue::GetImageToCache(
    const float device_scale_factor,
    const Document& document,
    const FetchParameters::ImageRequestBehavior image_request_behavior,
    const CrossOriginAttributeValue cross_origin,
    const CSSToLengthConversionData::ContainerSizes& container_sizes) {
  const ImageSetOption& best_option = GetBestOption(device_scale_factor);

  const CSSValue& image_value = Item(best_option.index);

  if (auto* image =
          const_cast<CSSImageValue*>(DynamicTo<CSSImageValue>(image_value))) {
    return image->CacheImage(document, image_request_behavior, cross_origin,
                             best_option.resolution);
  }

  if (!RuntimeEnabledFeatures::CSSImageSetEnabled()) {
    return nullptr;
  }

  if (auto* gradient = DynamicTo<cssvalue::CSSGradientValue>(image_value)) {
    return MakeGarbageCollected<StyleGeneratedImage>(*gradient,
                                                     container_sizes);
  }

  return nullptr;
}

String CSSImageSetValue::CustomCSSText() const {
  StringBuilder result;

  if (!RuntimeEnabledFeatures::CSSImageSetEnabled()) {
    result.Append("-webkit-");
  }

  result.Append("image-set(");

  for (wtf_size_t i = 0, length = this->length(); i < length; ++i) {
    if (i > 0) {
      result.Append(", ");
    }

    const CSSValue& image_value = Item(i);
    result.Append(image_value.CssText());
    result.Append(' ');

    ++i;
    SECURITY_DCHECK(i < length);
    const CSSValue& resolution_value = Item(i);
    result.Append(resolution_value.CssText());
  }

  result.Append(')');
  return result.ReleaseString();
}

bool CSSImageSetValue::HasFailedOrCanceledSubresources() const {
  if (!cached_image_) {
    return false;
  }
  if (ImageResourceContent* cached_content = cached_image_->CachedImage()) {
    return cached_content->LoadFailedOrCanceled();
  }
  return true;
}

void CSSImageSetValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(cached_image_);
  CSSValueList::TraceAfterDispatch(visitor);
}

CSSImageSetValue* CSSImageSetValue::ComputedCSSValue(
    const ComputedStyle& style,
    const bool allow_visited_style) const {
  auto* value = MakeGarbageCollected<CSSImageSetValue>();

  for (auto& item : *this) {
    value->Append(
        *ComputedCSSValueForOption(item.Get(), style, allow_visited_style));
  }

  return value;
}

const CSSValue* CSSImageSetValue::ComputedCSSValueForOption(
    const CSSValue* value,
    const ComputedStyle& style,
    const bool allow_visited_style) const {
  if (auto* image = DynamicTo<CSSImageValue>(value)) {
    return image->ComputedCSSValue();
  }

  if (RuntimeEnabledFeatures::CSSImageSetEnabled()) {
    if (auto* resolution = DynamicTo<CSSNumericLiteralValue>(value);
        resolution && resolution->IsResolution() &&
        resolution->GetType() != CSSPrimitiveValue::UnitType::kDotsPerPixel) {
      return CSSNumericLiteralValue::Create(
          resolution->ComputeDotsPerPixel(),
          CSSPrimitiveValue::UnitType::kDotsPerPixel);
    }

    if (auto* gradient = DynamicTo<cssvalue::CSSGradientValue>(value)) {
      return gradient->ComputedCSSValue(style, allow_visited_style);
    }
  }

  return value;
}

}  // namespace blink
