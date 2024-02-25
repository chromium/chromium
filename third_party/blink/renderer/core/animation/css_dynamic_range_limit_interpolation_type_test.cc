// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_dynamic_range_limit_interpolation_type.h"
#include <memory>
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/interpolable_dynamic_range_limit.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_recalc_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {
namespace {

class CSSDynamicRangeLimitInterpolationTypeTest : public PageTestBase {
 protected:
  std::unique_ptr<CSSDynamicRangeLimitInterpolationType>
  CreateDynamicRangeLimitInterpolationType() {
    ScopedCSSDynamicRangeLimitForTest scoped_feature(true);
    const CSSProperty& css_property =
        CSSProperty::Get(CSSPropertyID::kDynamicRangeLimit);
    PropertyHandle property = PropertyHandle(css_property);
    return std::make_unique<CSSDynamicRangeLimitInterpolationType>(property);
  }
};

TEST_F(CSSDynamicRangeLimitInterpolationTypeTest,
       MaybeConvertStandardPropertyUnderlyingValue) {
  SetBodyInnerHTML(R"HTML(
  <style>
    div {
      dynamic-range-limit: standard;
      transition: dynamic-range-limit 2s;
    }
  </style>
  <div id="text">Filler text</div>
  )HTML");
  Document& document = GetDocument();
  Element* element = document.getElementById(AtomicString("text"));
  StyleResolverState state(document, *element, nullptr,
                           StyleRequest(element->GetComputedStyle()));

  std::unique_ptr<CSSDynamicRangeLimitInterpolationType>
      dynamic_range_limit_interpolation_type =
          CreateDynamicRangeLimitInterpolationType();

  InterpolationValue result = dynamic_range_limit_interpolation_type
                                  ->MaybeConvertStandardPropertyUnderlyingValue(
                                      *element->GetComputedStyle());

  const InterpolableDynamicRangeLimit* interpolable_limit =
      To<InterpolableDynamicRangeLimit>(result.interpolable_value.Get());
  DynamicRangeLimit limit = interpolable_limit->GetDynamicRangeLimit();

  EXPECT_EQ(limit,
            DynamicRangeLimit(cc::PaintFlags::DynamicRangeLimit::kStandard));
}

TEST_F(CSSDynamicRangeLimitInterpolationTypeTest, MaybeConvertValue) {
  std::unique_ptr<CSSDynamicRangeLimitInterpolationType>
      dynamic_range_limit_interpolation_type =
          CreateDynamicRangeLimitInterpolationType();
  CSSDynamicRangeLimitInterpolationType::ConversionCheckers conversion_checkers;
  CSSValue* value =
      MakeGarbageCollected<CSSIdentifierValue>(CSSValueID::kStandard);

  InterpolationValue result =
      dynamic_range_limit_interpolation_type->MaybeConvertValue(
          *value, nullptr, conversion_checkers);

  const InterpolableDynamicRangeLimit* interpolable_limit =
      To<InterpolableDynamicRangeLimit>(result.interpolable_value.Get());
  DynamicRangeLimit limit = interpolable_limit->GetDynamicRangeLimit();

  EXPECT_EQ(limit,
            DynamicRangeLimit(cc::PaintFlags::DynamicRangeLimit::kStandard));
}

}  // namespace
}  // namespace blink
