// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/compositor_clip_path_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/basic_shape_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

// Tests that ensure blink interpolation and compositor interpolation of
// clip-path values are equivalent.

class CompositorClipPathValueTest : public PageTestBase {
 protected:
  const BasicShape* ParseBasicShape(const String& value) {
    const CSSValue* parsed = CSSParser::ParseSingleValue(
        CSSPropertyID::kClipPath, value,
        MakeGarbageCollected<CSSParserContext>(GetDocument()));
    const auto* list = DynamicTo<CSSValueList>(parsed);
    if (!list) {
      return nullptr;
    }
    StyleResolverState state(GetDocument(), *GetDocument().documentElement());
    return BasicShapeForValue(state, list->First());
  }

  InterpolationValue ConvertForBlink(const BasicShape& shape) {
    BasicShapeInfo info;
    info.shape = &shape;
    info.box = GeometryBox::kBorderBox;
    return basic_shape_interpolation_functions::MaybeConvertBasicShape(
        info, GetCSSPropertyClipPath(), /*zoom=*/1);
  }

  // For two given BasicShapes, produces CompositorClipPathValues and
  // InterpolationValues from both, and interpolates them separately.
  // The results are expected to be equal, accounting floating-point errors.
  void ExpectInterpolationMatchesBlink(const BasicShape& from,
                                       const BasicShape& to,
                                       float progress) {
    CompositorClipPathValue compositor_from =
        CompositorClipPathValue::FromShape(from, reference_box_, clip_offset_,
                                           /*zoom=*/1);
    CompositorClipPathValue compositor_to =
        CompositorClipPathValue::FromShape(to, reference_box_, clip_offset_,
                                           /*zoom=*/1);
    CompositorClipPathValue compositor_result;
    compositor_from.Interpolate(compositor_to, progress, &compositor_result);

    InterpolationValue blink_from = ConvertForBlink(from);
    InterpolationValue blink_to = ConvertForBlink(to);
    ASSERT_TRUE(basic_shape_interpolation_functions::ShapesAreCompatible(
        *blink_from.non_interpolable_value, *blink_to.non_interpolable_value));

    InterpolableValue* blink_interpolated =
        blink_from.interpolable_value->CloneAndZero();
    blink_from.interpolable_value->Interpolate(*blink_to.interpolable_value,
                                               progress, *blink_interpolated);
    CSSToLengthConversionData conversion_data(/*element=*/nullptr);
    BasicShapeInfo blink_result =
        basic_shape_interpolation_functions::CreateBasicShape(
            *blink_interpolated, *blink_from.non_interpolable_value,
            conversion_data);
    ASSERT_TRUE(blink_result.shape);

    CompositorClipPathValue expected = CompositorClipPathValue::FromShape(
        *blink_result.shape, reference_box_, clip_offset_, /*zoom=*/1);
    EXPECT_TRUE(compositor_result.IsRoughlyEqualForTesting(expected));
  }

  const gfx::RectF reference_box_{10, 20, 200, 150};
  const gfx::Vector2dF clip_offset_{7, 11};
};

TEST_F(CompositorClipPathValueTest, InsetInterpolationMatchesBlink) {
  const BasicShape* from = ParseBasicShape(
      "inset(10px 20px 30px 40px round "
      "4px 6px 8px 10px / 5px 7px 9px 11px)");
  const BasicShape* to = ParseBasicShape(
      "inset(25px 5px 15px 20px round "
      "12px 14px 16px 18px / 13px 15px 17px 19px)");

  ExpectInterpolationMatchesBlink(*from, *to, 0.25f);
}

TEST_F(CompositorClipPathValueTest, PolygonInterpolationMatchesBlink) {
  const BasicShape* from =
      ParseBasicShape("polygon(10px 20px, 150px 5px, 180px 110px, 25px 130px)");
  const BasicShape* to =
      ParseBasicShape("polygon(30px 5px, 175px 35px, 140px 135px, 5px 100px)");

  ExpectInterpolationMatchesBlink(*from, *to, 0.25f);
}

TEST_F(CompositorClipPathValueTest, RectFDoesNotInterpolateWithInset) {
  const BasicShape* inset_shape =
      ParseBasicShape("inset(10px 20px 30px 40px round 4px 6px 8px 10px)");

  CompositorClipPathValue rect =
      CompositorClipPathValue::FromRectF(gfx::RectF(0, 0, 20, 20));
  CompositorClipPathValue inset = CompositorClipPathValue::FromShape(
      *inset_shape, reference_box_, clip_offset_, /*zoom=*/1);
  CompositorClipPathValue result;

  rect.Interpolate(inset, 0.25f, &result);
  EXPECT_TRUE(result.IsRoughlyEqualForTesting(rect));

  rect.Interpolate(inset, 0.75f, &result);
  EXPECT_TRUE(result.IsRoughlyEqualForTesting(inset));
}

}  // namespace blink
