// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_image_set_option_value.h"

#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

const CSSValue* ComputeImage(const CSSValue* value,
                             const ComputedStyle& style,
                             const bool allow_visited_style) {
  if (auto* image = DynamicTo<CSSImageValue>(value)) {
    return image->ComputedCSSValue();
  }

  if (!RuntimeEnabledFeatures::CSSImageSetEnabled()) {
    return value;
  }

  if (auto* gradient = DynamicTo<cssvalue::CSSGradientValue>(value)) {
    return gradient->ComputedCSSValue(style, allow_visited_style);
  }

  NOTREACHED();

  return value;
}

const CSSNumericLiteralValue* ComputeResolution(
    const CSSNumericLiteralValue* resolution) {
  if (RuntimeEnabledFeatures::CSSImageSetEnabled() && resolution &&
      resolution->IsResolution() &&
      resolution->GetType() != CSSPrimitiveValue::UnitType::kDotsPerPixel) {
    return CSSNumericLiteralValue::Create(
        resolution->ComputeDotsPerPixel(),
        CSSPrimitiveValue::UnitType::kDotsPerPixel);
  }

  return resolution;
}

}  // namespace

CSSImageSetOptionValue::CSSImageSetOptionValue(
    const CSSValue* image,
    const CSSNumericLiteralValue* resolution)
    : CSSValue(kImageSetOptionClass), image_(image), resolution_(resolution) {
  DCHECK(image);

  if (!resolution_) {
    resolution_ =
        CSSNumericLiteralValue::Create(1.0, CSSPrimitiveValue::UnitType::kX);
  }
}

CSSImageSetOptionValue::~CSSImageSetOptionValue() = default;

StyleImage* CSSImageSetOptionValue::CacheImage(
    const Document& document,
    const FetchParameters::ImageRequestBehavior image_request_behavior,
    const CrossOriginAttributeValue cross_origin,
    const CSSToLengthConversionData::ContainerSizes& container_sizes) const {
  if (auto* image =
          const_cast<CSSImageValue*>(DynamicTo<CSSImageValue>(image_.Get()))) {
    return image->CacheImage(document, image_request_behavior, cross_origin,
                             ComputedResolution());
  }

  if (!RuntimeEnabledFeatures::CSSImageSetEnabled()) {
    return nullptr;
  }

  if (auto* gradient = DynamicTo<cssvalue::CSSGradientValue>(image_.Get())) {
    return MakeGarbageCollected<StyleGeneratedImage>(*gradient,
                                                     container_sizes);
  }

  NOTREACHED();

  return nullptr;
}

double CSSImageSetOptionValue::ComputedResolution() const {
  return resolution_->ComputeDotsPerPixel();
}

String CSSImageSetOptionValue::CustomCSSText() const {
  StringBuilder result;

  result.Append(image_->CssText());
  result.Append(' ');
  result.Append(resolution_->CssText());

  return result.ReleaseString();
}

bool CSSImageSetOptionValue::Equals(const CSSImageSetOptionValue& other) const {
  return *image_ == *other.image_ && *resolution_ == *other.resolution_;
}

CSSImageSetOptionValue* CSSImageSetOptionValue::ComputedCSSValue(
    const ComputedStyle& style,
    const bool allow_visited_style) const {
  return MakeGarbageCollected<CSSImageSetOptionValue>(
      ComputeImage(image_, style, allow_visited_style),
      ComputeResolution(resolution_));
}

void CSSImageSetOptionValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(image_);
  visitor->Trace(resolution_);

  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
