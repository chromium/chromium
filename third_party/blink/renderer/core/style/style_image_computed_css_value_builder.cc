// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_image_computed_css_value_builder.h"

#include "third_party/blink/renderer/core/css/css_crossfade_value.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_image_generator_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_option_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"

namespace blink {

namespace {

const CSSPrimitiveValue* ComputeResolution(
    const CSSPrimitiveValue& resolution) {
  if (resolution.IsResolution()) {
    return CSSNumericLiteralValue::Create(
        resolution.ComputeDotsPerPixel(),
        CSSPrimitiveValue::UnitType::kDotsPerPixel);
  }
  return &resolution;
}

}  // namespace

CSSValue* StyleImageComputedCSSValueBuilder::CrossfadeArgument(
    CSSValue* value) const {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return value;
  }
  return Build(value);
}

CSSValue* StyleImageComputedCSSValueBuilder::BuildImageSet(
    const CSSImageSetValue& image_set_value) const {
  auto* computed_value = MakeGarbageCollected<CSSImageSetValue>();
  for (const auto& list_item : image_set_value) {
    auto& option = To<CSSImageSetOptionValue>(*list_item);
    auto* computed_option = MakeGarbageCollected<CSSImageSetOptionValue>(
        Build(&option.GetImage()), ComputeResolution(option.GetResolution()),
        option.GetType());
    computed_value->Append(*computed_option);
  }
  return computed_value;
}

CSSValue* StyleImageComputedCSSValueBuilder::Build(CSSValue* value) const {
  if (auto* image_value = DynamicTo<CSSImageValue>(value)) {
    return image_value->ComputedCSSValue();
  }
  if (auto* image_set_value = DynamicTo<CSSImageSetValue>(value)) {
    return BuildImageSet(*image_set_value);
  }
  if (auto* image_crossfade = DynamicTo<cssvalue::CSSCrossfadeValue>(value)) {
    HeapVector<std::pair<Member<CSSValue>, Member<CSSPrimitiveValue>>>
        images_and_percentages;
    for (const auto& [image, percentage] :
         image_crossfade->GetImagesAndPercentages()) {
      images_and_percentages.emplace_back(CrossfadeArgument(image), percentage);
    }
    return MakeGarbageCollected<cssvalue::CSSCrossfadeValue>(
        image_crossfade->IsPrefixedVariant(),
        std::move(images_and_percentages));
  }
  if (IsA<CSSPaintValue>(value)) {
    return value;
  }
  if (auto* image_gradient_value =
          DynamicTo<cssvalue::CSSGradientValue>(value)) {
    return image_gradient_value->ComputedCSSValue(style_, allow_visited_style_,
                                                  value_phase_);
  }
  NOTREACHED_IN_MIGRATION();
  return value;
}

}  // namespace blink
