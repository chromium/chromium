// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_image_set_option_value.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/css_image_set_type_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

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

double CSSImageSetOptionValue::ComputedResolution(
    const CSSLengthResolver& resolver) const {
  return resolution_->ComputeDotsPerPixel(resolver);
}

bool CSSImageSetOptionValue::IsSupported(
    const CSSLengthResolver& resolver) const {
  return (!type_ || type_->IsSupported()) &&
         (ComputedResolution(resolver) > 0.0);
}

CSSValue& CSSImageSetOptionValue::GetImage() const {
  return const_cast<CSSValue&>(*image_);
}

const CSSPrimitiveValue& CSSImageSetOptionValue::GetResolution() const {
  return *resolution_;
}

const CSSImageSetTypeValue* CSSImageSetOptionValue::GetType() const {
  return type_.Get();
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

const CSSValue*
CSSImageSetOptionValue::CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
    const CSSPropertyName& property_name,
    wtf_size_t property_value_index) const {
  const CSSValue* image =
      image_ ? image_->CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
                   property_name, property_value_index)
             : nullptr;
  const CSSPrimitiveValue* resolution =
      resolution_
          ? To<CSSPrimitiveValue>(
                resolution_
                    ->CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
                        property_name, property_value_index))
          : nullptr;
  if (image != image_ || resolution != resolution_) {
    return MakeGarbageCollected<CSSImageSetOptionValue>(image, resolution,
                                                        type_);
  }
  return this;
}

void CSSImageSetOptionValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(image_);
  visitor->Trace(resolution_);
  visitor->Trace(type_);

  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
