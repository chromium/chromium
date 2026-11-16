// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_opacity_interpolation_type.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_request.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
namespace {

class CSSOpacityInterpolationTypeTest : public PageTestBase {};

TEST_F(CSSOpacityInterpolationTypeTest, AutoDoesNotConvertForHighlight) {
  auto* interpolation_type = MakeGarbageCollected<CSSOpacityInterpolationType>(
      PropertyHandle(GetCSSPropertyOpacity()));
  const ComputedStyle& initial_style =
      GetDocument().GetStyleResolver().InitialStyle();
  StyleRequest request(kPseudoIdSelection, &initial_style, &initial_style);
  StyleResolverState state(GetDocument(), *GetDocument().documentElement(),
                           nullptr, request);
  CSSInterpolationType::ConversionCheckers conversion_checkers;

  InterpolationValue result = interpolation_type->MaybeConvertValue(
      *CSSIdentifierValue::Create(CSSValueID::kAuto), state,
      conversion_checkers);

  EXPECT_FALSE(result);
  EXPECT_TRUE(conversion_checkers.empty());
}

}  // namespace
}  // namespace blink
