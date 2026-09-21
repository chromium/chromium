// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_gradient_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/graphics/gradient.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

using CSSGradientValue = cssvalue::CSSGradientValue;

const CSSGradientValue* ParseSingleGradient(const char* text) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage, text,
      StrictCSSParserContext(SecureContextMode::kInsecureContext));
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    DCHECK_EQ(list->length(), 1u);
    return &To<CSSGradientValue>(list->Item(0));
  }
  NOTREACHED();
}

bool CompareGradients(const char* gradient1, const char* gradient2) {
  const CSSValue* value1 = ParseSingleGradient(gradient1);
  const CSSValue* value2 = ParseSingleGradient(gradient2);
  return *value1 == *value2;
}

CSSPrimitiveValue::LengthTypeFlags LengthUnitTypes(const char* text) {
  const CSSGradientValue* gradient = ParseSingleGradient(text);
  CSSPrimitiveValue::LengthTypeFlags types;
  gradient->AccumulateLengthUnitTypes(types);
  return types;
}

bool IsUsingContainerRelativeUnits(const char* text) {
  return CSSPrimitiveValue::HasContainerRelativeUnits(LengthUnitTypes(text));
}

bool IsUsingStaticViewportUnits(const char* text) {
  return CSSPrimitiveValue::HasStaticViewportUnits(LengthUnitTypes(text));
}

bool IsUsingDynamicViewportUnits(const char* text) {
  return CSSPrimitiveValue::HasDynamicViewportUnits(LengthUnitTypes(text));
}

TEST(CSSGradientValueTest, RadialGradient_Equals) {
  test::TaskEnvironment task_environment;
  // Trivially identical.
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(circle closest-corner at 100px 60px, blue, red)",
      "radial-gradient(circle closest-corner at 100px 60px, blue, red)"));
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(100px 150px at 100px 60px, blue, red)",
      "radial-gradient(100px 150px at 100px 60px, blue, red)"));

  // Identical with differing parameterization.
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(100px 150px at 100px 60px, blue, red)",
      "radial-gradient(ellipse 100px 150px at 100px 60px, blue, red)"));
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(100px at 100px 60px, blue, red)",
      "radial-gradient(circle 100px at 100px 60px, blue, red)"));
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(closest-corner at 100px 60px, blue, red)",
      "radial-gradient(ellipse closest-corner at 100px 60px, blue, red)"));
  EXPECT_TRUE(CompareGradients(
      "radial-gradient(ellipse at 100px 60px, blue, red)",
      "radial-gradient(ellipse farthest-corner at 100px 60px, blue, red)"));

  // Different.
  EXPECT_FALSE(CompareGradients(
      "radial-gradient(circle closest-corner at 100px 60px, blue, red)",
      "radial-gradient(circle farthest-side  at 100px 60px, blue, red)"));
  EXPECT_FALSE(CompareGradients(
      "radial-gradient(circle at 100px 60px, blue, red)",
      "radial-gradient(circle farthest-side  at 100px 60px, blue, red)"));
  EXPECT_FALSE(CompareGradients(
      "radial-gradient(100px 150px at 100px 60px, blue, red)",
      "radial-gradient(circle farthest-side  at 100px 60px, blue, red)"));
  EXPECT_FALSE(
      CompareGradients("radial-gradient(100px 150px at 100px 60px, blue, red)",
                       "radial-gradient(100px at 100px 60px, blue, red)"));
}

TEST(CSSGradientValueTest, RepeatingRadialGradientNan) {
  test::TaskEnvironment task_environment;
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>();
  Document& document = dummy_page_holder->GetDocument();
  CSSToLengthConversionData conversion_data(/*element=*/nullptr);

  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage,
      "-webkit-repeating-radial-gradient(center, deeppink -7%, gray "
      "3.40282e+38%)",
      StrictCSSParserContext(SecureContextMode::kInsecureContext));

  auto* value_list = DynamicTo<CSSValueList>(value);
  ASSERT_TRUE(value_list);

  auto* radial =
      DynamicTo<cssvalue::CSSRadialGradientValue>(value_list->Last());
  ASSERT_TRUE(radial);

  // This should not fail any DCHECKs.
  radial->CreateGradient(
      conversion_data, gfx::SizeF(800, 200), document,
      document.GetStyleEngine().GetStyleResolver().InitialStyle());
}

TEST(CSSGradientValueTest, IsUsingContainerRelativeUnits) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(
      IsUsingContainerRelativeUnits("linear-gradient(green 5cqw, blue 10cqh)"));
  EXPECT_TRUE(
      IsUsingContainerRelativeUnits("linear-gradient(green 5cqi, blue 10cqb)"));
  EXPECT_TRUE(IsUsingContainerRelativeUnits(
      "linear-gradient(green 5cqmin, blue 10cqmax)"));
  EXPECT_TRUE(
      IsUsingContainerRelativeUnits("linear-gradient(green 10px, blue 10cqh)"));
  EXPECT_TRUE(
      IsUsingContainerRelativeUnits("linear-gradient(green 5cqw, blue 10px)"));
  EXPECT_TRUE(
      IsUsingContainerRelativeUnits("radial-gradient(green 5cqw, blue 10cqh)"));
  EXPECT_TRUE(
      IsUsingContainerRelativeUnits("radial-gradient(green 10px, blue 10cqh)"));
  EXPECT_TRUE(
      IsUsingContainerRelativeUnits("radial-gradient(green 5cqw, blue 10px)"));
  EXPECT_TRUE(IsUsingContainerRelativeUnits(
      "conic-gradient(from 180deg at 10cqh 20cqw, green, blue)"));
  EXPECT_TRUE(IsUsingContainerRelativeUnits(
      "conic-gradient(from 180deg at 10px 20cqw, green, blue)"));
  EXPECT_TRUE(IsUsingContainerRelativeUnits(
      "conic-gradient(from 180deg at 10cqh 20px, green, blue)"));
  EXPECT_TRUE(IsUsingContainerRelativeUnits(
      "linear-gradient(green calc(10px + 5cqw), blue 10px)"));

  EXPECT_FALSE(
      IsUsingContainerRelativeUnits("linear-gradient(green 10px, blue 10vh)"));
  EXPECT_FALSE(
      IsUsingContainerRelativeUnits("linear-gradient(green 10px, blue 10em)"));
  EXPECT_FALSE(IsUsingContainerRelativeUnits(
      "linear-gradient(green calc(10px + 20em), blue 10px)"));
  EXPECT_FALSE(
      IsUsingContainerRelativeUnits("radial-gradient(green 5px, blue 10px)"));
  EXPECT_FALSE(IsUsingContainerRelativeUnits(
      "conic-gradient(from 180deg at 10px 20px, green, blue)"));
}

TEST(CSSGradientValueTest, IsUsingStaticViewportUnits) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(
      IsUsingStaticViewportUnits("linear-gradient(green 5vw, blue 10vh)"));
  EXPECT_TRUE(
      IsUsingStaticViewportUnits("linear-gradient(green 5vi, blue 10vb)"));
  EXPECT_TRUE(
      IsUsingStaticViewportUnits("linear-gradient(green 5vmin, blue 10vmax)"));
  EXPECT_TRUE(
      IsUsingStaticViewportUnits("linear-gradient(green 5svw, blue 10svh)"));
  EXPECT_TRUE(
      IsUsingStaticViewportUnits("linear-gradient(green 5lvw, blue 10lvh)"));
  EXPECT_TRUE(
      IsUsingStaticViewportUnits("linear-gradient(green 10px, blue 10vh)"));
  EXPECT_TRUE(
      IsUsingStaticViewportUnits("linear-gradient(green 5vw, blue 10px)"));
  EXPECT_TRUE(IsUsingStaticViewportUnits(
      "linear-gradient(green calc(10px + 5vw), blue 10px)"));

  // Radial gradient positions and sizes resolve viewport units too.
  EXPECT_TRUE(
      IsUsingStaticViewportUnits("radial-gradient(green 5vw, blue 10vh)"));
  EXPECT_TRUE(IsUsingStaticViewportUnits(
      "radial-gradient(circle 40vh at 50% 50%, green, blue)"));
  EXPECT_TRUE(IsUsingStaticViewportUnits(
      "radial-gradient(circle 40px at 10vh 20px, green, blue)"));

  // As do conic gradient positions.
  EXPECT_TRUE(IsUsingStaticViewportUnits(
      "conic-gradient(from 180deg at 10vh 20vw, green, blue)"));
  EXPECT_TRUE(IsUsingStaticViewportUnits(
      "conic-gradient(from 180deg at 10px 20vw, green, blue)"));

  EXPECT_FALSE(
      IsUsingStaticViewportUnits("linear-gradient(green 10px, blue 10em)"));
  EXPECT_FALSE(
      IsUsingStaticViewportUnits("linear-gradient(green 10px, blue 10cqh)"));
  EXPECT_FALSE(
      IsUsingStaticViewportUnits("linear-gradient(green 10px, blue 50%)"));
  EXPECT_FALSE(
      IsUsingStaticViewportUnits("radial-gradient(green 5px, blue 10px)"));
  EXPECT_FALSE(IsUsingStaticViewportUnits(
      "conic-gradient(from 180deg at 10px 20px, green, blue)"));

  // Dynamic viewport units are reported separately.
  EXPECT_FALSE(
      IsUsingStaticViewportUnits("linear-gradient(green 10px, blue 10dvh)"));
}

TEST(CSSGradientValueTest, IsUsingDynamicViewportUnits) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(
      IsUsingDynamicViewportUnits("linear-gradient(green 5dvw, blue 10dvh)"));
  EXPECT_TRUE(IsUsingDynamicViewportUnits(
      "linear-gradient(green 5dvmin, blue 10dvmax)"));
  EXPECT_TRUE(IsUsingDynamicViewportUnits(
      "linear-gradient(green calc(10px + 5dvw), blue 10px)"));
  EXPECT_TRUE(IsUsingDynamicViewportUnits(
      "radial-gradient(circle 40dvh at 50% 50%, green, blue)"));
  EXPECT_TRUE(IsUsingDynamicViewportUnits(
      "conic-gradient(from 180deg at 10dvh 20px, green, blue)"));

  EXPECT_FALSE(
      IsUsingDynamicViewportUnits("linear-gradient(green 10px, blue 10vh)"));
  EXPECT_FALSE(
      IsUsingDynamicViewportUnits("linear-gradient(green 10px, blue 10svh)"));
  EXPECT_FALSE(
      IsUsingDynamicViewportUnits("linear-gradient(green 10px, blue 10px)"));
}

}  // namespace

}  // namespace blink
