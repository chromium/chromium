// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_image_set_option_value.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
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

const CSSPrimitiveValue* ComputeResolution(
    const CSSPrimitiveValue* resolution) {
  if (RuntimeEnabledFeatures::CSSImageSetEnabled() && resolution &&
      resolution->IsResolution()) {
    return CSSNumericLiteralValue::Create(
        resolution->ComputeDotsPerPixel(),
        CSSPrimitiveValue::UnitType::kDotsPerPixel);
  }

  return resolution;
}

}  // namespace

CSSImageSetOptionValue::CSSImageSetOptionValue(
    const CSSValue* image,
    const CSSPrimitiveValue* resolution,
    const CSSImageSetTypeValue* type)
    : CSSValue(kImageSetOptionClass),
      image_(image),
      resolution_(resolution),
      type_(type) {
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

bool CSSImageSetOptionValue::IsSupported() const {
  return (!type_ || type_->IsSupported()) &&
         (resolution_->ComputeDotsPerPixel() > 0.0);
}

String CSSImageSetOptionValue::CustomCSSText() const {
  StringBuilder result;

  result.Append(image_->CssText());
  result.Append(' ');
  result.Append(resolution_->CssText());
  if (type_) {
    result.Append(' ');
    result.Append(type_->CssText());
  }

  return result.ReleaseString();
}

bool CSSImageSetOptionValue::Equals(const CSSImageSetOptionValue& other) const {
  return base::ValuesEquivalent(image_, other.image_) &&
         base::ValuesEquivalent(resolution_, other.resolution_) &&
         base::ValuesEquivalent(type_, other.type_);
}

CSSImageSetOptionValue* CSSImageSetOptionValue::ComputedCSSValue(
    const ComputedStyle& style,
    const bool allow_visited_style) const {
  return MakeGarbageCollected<CSSImageSetOptionValue>(
      ComputeImage(image_, style, allow_visited_style),
      ComputeResolution(resolution_), type_);
}

void CSSImageSetOptionValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(image_);
  visitor->Trace(resolution_);
  visitor->Trace(type_);

  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
