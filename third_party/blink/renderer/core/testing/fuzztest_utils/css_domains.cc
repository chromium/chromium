// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fuzztest_utils/css_domains.h"

#include <string>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value_id_mappings.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/common_domains.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

fuzztest::Domain<std::string> AnyCSSLengthValue();

namespace {

template <typename E>
std::string CSSEnumToString(int val) {
  E enum_val = static_cast<E>(val);
  return base::ToString(enum_val);
}

fuzztest::Domain<std::string> AnyCSSPercentageValue() {
  return fuzztest::Map(
      [](float val) { return base::StrCat({base::NumberToString(val), "%"}); },
      fuzztest::Finite<float>());
}

fuzztest::Domain<std::string> AnyCSSLengthPercentageValue() {
  auto numeric = fuzztest::Map(
      [](float val, const std::string& unit) {
        return base::StrCat({base::NumberToString(val), unit});
      },
      fuzztest::Finite<float>(),
      fuzztest::ElementOf<std::string>({"px", "%", "em", "vh", "vw"}));
  auto calc = fuzztest::Map(
      [](const std::string& a, const std::string& b) {
        return base::StrCat({"calc(", a, " + ", b, ")"});
      },
      AnyCSSLengthValue(), AnyCSSPercentageValue());
  return fuzztest::OneOf(numeric, calc);
}

fuzztest::Domain<std::string> AnyCSSAngleValueIncludingUnitlessZero() {
  return fuzztest::OneOf(
      fuzztest::Just(std::string("0")),
      fuzztest::Map(
          [](float val, const std::string& unit) {
            return base::StrCat({base::NumberToString(val), unit});
          },
          fuzztest::Finite<float>(),
          fuzztest::ElementOf<std::string>({"deg", "rad"})));
}

fuzztest::Domain<std::string> AnyCSSTransformScaleFactorValue() {
  return fuzztest::OneOf(
      fuzztest::Map([](float val) { return base::NumberToString(val); },
                    fuzztest::Finite<float>()),
      fuzztest::Map(
          [](float val) {
            return base::StrCat({base::NumberToString(val), "%"});
          },
          fuzztest::Finite<float>()));
}

fuzztest::Domain<std::string> AnyCSSTranslateValue() {
  auto length = AnyCSSLengthValue();
  auto length_or_percentage = AnyCSSLengthPercentageValue();
  // translateX() = translateX( <length-percentage> )
  // translateY() = translateY( <length-percentage> )
  auto translate_1arg = fuzztest::Map(
      [](const std::string& fn, const std::string& val) {
        return base::StrCat({fn, "(", val, ")"});
      },
      fuzztest::ElementOf<std::string>({"translateX", "translateY"}),
      length_or_percentage);
  // translate() = translate( <length-percentage> )
  auto translate = fuzztest::Map(
      [](const std::string& val) {
        return base::StrCat({"translate(", val, ")"});
      },
      length_or_percentage);
  // translateZ() = translateZ( <length> )
  auto translate_z = fuzztest::Map(
      [](const std::string& val) {
        return base::StrCat({"translateZ(", val, ")"});
      },
      length);
  // translate() = translate( <length-percentage> , <length-percentage>? )
  auto translate_2arg = fuzztest::Map(
      [](const std::string& x, const std::string& y) {
        return base::StrCat({"translate(", x, ", ", y, ")"});
      },
      length_or_percentage, length_or_percentage);
  // translate3d() = translate3d( <length-percentage> ,
  //                              <length-percentage> , <length> )
  auto translate_3arg = fuzztest::Map(
      [](const std::string& x, const std::string& y, const std::string& z) {
        return base::StrCat({"translate3d(", x, ", ", y, ", ", z, ")"});
      },
      length_or_percentage, length_or_percentage, length);
  return fuzztest::OneOf(translate_1arg, translate, translate_z, translate_2arg,
                         translate_3arg);
}

fuzztest::Domain<std::string> AnyCSSScaleValue() {
  auto scale_factor = AnyCSSTransformScaleFactorValue();
  // scale() = scale( [ <number> | <percentage> ]#{1,2} )
  // scaleX() = scaleX( [ <number> | <percentage> ] )
  // scaleY() = scaleY( [ <number> | <percentage> ] )
  // scaleZ() = scaleZ( [ <number> | <percentage> ] )
  auto scale_1arg = fuzztest::Map(
      [](const std::string& fn, const std::string& val) {
        return base::StrCat({fn, "(", val, ")"});
      },
      fuzztest::ElementOf<std::string>({"scale", "scaleX", "scaleY", "scaleZ"}),
      scale_factor);
  // scale() = scale( [ <number> | <percentage> ]#{1,2} )
  auto scale_2arg = fuzztest::Map(
      [](const std::string& sx, const std::string& sy) {
        return base::StrCat({"scale(", sx, ", ", sy, ")"});
      },
      scale_factor, scale_factor);
  // scale3d() = scale3d( [ <number> | <percentage> ]#{3} )
  auto scale_3arg = fuzztest::Map(
      [](const std::string& sx, const std::string& sy, const std::string& sz) {
        return base::StrCat({"scale3d(", sx, ", ", sy, ", ", sz, ")"});
      },
      scale_factor, scale_factor, scale_factor);
  return fuzztest::OneOf(scale_1arg, scale_2arg, scale_3arg);
}

fuzztest::Domain<std::string> AnyCSSRotateValue() {
  auto angle = AnyCSSAngleValueIncludingUnitlessZero();
  // rotate() = rotate( [ <angle> | <zero> ] )
  // rotateX() = rotateX( [ <angle> | <zero> ] )
  // rotateY() = rotateY( [ <angle> | <zero> ] )
  // rotateZ() = rotateZ( [ <angle> | <zero> ] )
  auto rotate_1arg = fuzztest::Map(
      [](const std::string& fn, const std::string& val) {
        return base::StrCat({fn, "(", val, ")"});
      },
      fuzztest::ElementOf<std::string>(
          {"rotate", "rotateX", "rotateY", "rotateZ"}),
      angle);
  // rotate3d() = rotate3d( <number> , <number> , <number> ,
  //                        [ <angle> | <zero> ] )
  auto rotate_3d = fuzztest::Map(
      [](int x, int y, int z, const std::string& a) {
        return base::StrCat({"rotate3d(", base::NumberToString(x), ", ",
                             base::NumberToString(y), ", ",
                             base::NumberToString(z), ", ", a, ")"});
      },
      fuzztest::InRange(-1, 1), fuzztest::InRange(-1, 1),
      fuzztest::InRange(-1, 1), angle);
  return fuzztest::OneOf(rotate_1arg, rotate_3d);
}

fuzztest::Domain<std::string> AnyCSSSkewValue() {
  auto angle = AnyCSSAngleValueIncludingUnitlessZero();
  // skew() = skew( [ <angle> | <zero> ] , [ <angle> | <zero> ]? )
  // skewX() = skewX( [ <angle> | <zero> ] )
  // skewY() = skewY( [ <angle> | <zero> ] )
  auto skew_1arg = fuzztest::Map(
      [](const std::string& fn, const std::string& val) {
        return base::StrCat({fn, "(", val, ")"});
      },
      fuzztest::ElementOf<std::string>({"skew", "skewX", "skewY"}), angle);
  // skew() = skew( [ <angle> | <zero> ] , [ <angle> | <zero> ]? )
  auto skew_2arg = fuzztest::Map(
      [](const std::string& ax, const std::string& ay) {
        return base::StrCat({"skew(", ax, ", ", ay, ")"});
      },
      angle, angle);
  return fuzztest::OneOf(skew_1arg, skew_2arg);
}

fuzztest::Domain<std::string> AnyCSSPerspectiveValue() {
  // perspective() = perspective( [ <length [0,∞]> | none ] )
  auto perspective = fuzztest::Map(
      [](float val) {
        return base::StrCat({"perspective(", base::NumberToString(val), "px)"});
      },
      fuzztest::Positive<float>());
  return fuzztest::OneOf(fuzztest::Just(std::string("perspective(none)")),
                         fuzztest::Just(std::string("perspective(0)")),
                         perspective);
}

fuzztest::Domain<std::string> AnyCSSMatrixValue() {
  // matrix() = matrix( <number>#{6} )
  auto matrix = fuzztest::Map(
      [](double a, double b, double c, double d, double e, double f) {
        return base::StrCat(
            {"matrix(", base::NumberToString(a), ", ", base::NumberToString(b),
             ", ", base::NumberToString(c), ", ", base::NumberToString(d), ", ",
             base::NumberToString(e), ", ", base::NumberToString(f), ")"});
      },
      fuzztest::Finite<double>(), fuzztest::Finite<double>(),
      fuzztest::Finite<double>(), fuzztest::Finite<double>(),
      fuzztest::Finite<double>(), fuzztest::Finite<double>());
  // matrix3d() = matrix3d( <number>#{16} )
  auto matrix3d = fuzztest::Map(
      [](std::vector<double> vals) {
        std::vector<std::string> parts;
        for (double v : vals) {
          parts.push_back(base::NumberToString(v));
        }
        return base::StrCat({"matrix3d(", base::JoinString(parts, ", "), ")"});
      },
      fuzztest::VectorOf(fuzztest::Finite<double>()).WithSize(16));
  return fuzztest::OneOf(matrix, matrix3d);
}

fuzztest::Domain<std::string> AnyCSSCalcSumValue() {
  // <calc-sum> = <calc-product> [ [ '+' | '-' ] <calc-product> ]*
  // <calc-product> = <calc-value> [ [ '*' | / ] <calc-value> ]*
  // <calc-value> = <number> | <dimension> | <percentage> |
  //                <calc-keyword> | ( <calc-sum> )
  // <calc-keyword> = e | pi | infinity | -infinity | NaN
  // Use a bounded subset of this grammar for fuzzing.
  auto number = fuzztest::Map([](double v) { return base::NumberToString(v); },
                              fuzztest::Finite<double>());
  auto dimension = fuzztest::Map(
      [](int val, const std::string& unit) {
        return base::StrCat({base::NumberToString(val), unit});
      },
      fuzztest::InRange(0, 10000),
      fuzztest::ElementOf<std::string>({"px", "em", "vh", "vw"}));
  auto percentage = fuzztest::Map(
      [](int val) { return base::StrCat({base::NumberToString(val), "%"}); },
      fuzztest::InRange(0, 10000));
  auto calc_keyword = fuzztest::ElementOf<std::string>(
      {"e", "pi", "infinity", "-infinity", "NaN"});
  auto calc_value =
      fuzztest::OneOf(number, dimension, percentage, calc_keyword);
  auto calc_product = fuzztest::OneOf(
      calc_value, fuzztest::Map(
                      [](const std::string& lhs, const std::string& op,
                         const std::string& rhs) {
                        return base::StrCat({lhs, " ", op, " ", rhs});
                      },
                      calc_value, fuzztest::ElementOf<std::string>({"*", "/"}),
                      calc_value));
  return fuzztest::OneOf(
      calc_product,
      fuzztest::Map(
          [](const std::string& lhs, const std::string& op,
             const std::string& rhs) {
            return base::StrCat({lhs, " ", op, " ", rhs});
          },
          calc_product, fuzztest::ElementOf<std::string>({"+", "-"}),
          calc_product),
      fuzztest::Map(
          [](const std::string& inner) {
            return base::StrCat({"(", inner, ")"});
          },
          calc_product));
}

}  // namespace

fuzztest::Domain<CSSPropertyID> AnyCSSProperty() {
  return fuzztest::Map(
      [](int val) { return static_cast<CSSPropertyID>(val); },
      fuzztest::InRange(kIntFirstCSSProperty, kIntLastCSSProperty));
}

fuzztest::Domain<CSSValueID> AnyCSSValue() {
  return fuzztest::Map([](int val) { return static_cast<CSSValueID>(val); },
                       fuzztest::InRange(1, kNumCSSValueKeywords - 1));
}

fuzztest::Domain<std::string> AnyCSSDisplayValue() {
  return fuzztest::Map(
      CSSEnumToString<EDisplay>,
      fuzztest::InRange(0, static_cast<int>(EDisplay::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSPositionValue() {
  return fuzztest::Map(
      CSSEnumToString<EPosition>,
      fuzztest::InRange(0, static_cast<int>(EPosition::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSVisibilityValue() {
  return fuzztest::Map(
      CSSEnumToString<EVisibility>,
      fuzztest::InRange(0, static_cast<int>(EVisibility::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSContentVisibilityValue() {
  return fuzztest::Map(
      CSSEnumToString<EContentVisibility>,
      fuzztest::InRange(0,
                        static_cast<int>(EContentVisibility::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSOverflowValue() {
  return fuzztest::Map(
      CSSEnumToString<EOverflow>,
      fuzztest::InRange(0, static_cast<int>(EOverflow::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSTextOrientationValue() {
  return fuzztest::Map(
      CSSEnumToString<ETextOrientation>,
      fuzztest::InRange(0, static_cast<int>(ETextOrientation::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSFlexDirectionValue() {
  return fuzztest::Map(
      CSSEnumToString<EFlexDirection>,
      fuzztest::InRange(0, static_cast<int>(EFlexDirection::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSReadingFlowValue() {
  return fuzztest::Map(
      CSSEnumToString<EReadingFlow>,
      fuzztest::InRange(0, static_cast<int>(EReadingFlow::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSTextOverflowValue() {
  return fuzztest::OneOf(fuzztest::ElementOf<std::string>({"clip", "ellipsis"}),
                         fuzztest::Arbitrary<std::string>());
}

fuzztest::Domain<std::string> AnyCSSAnimationDirectionValue() {
  return fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      fuzztest::ElementOf<CSSValueID>(
          {CSSValueID::kNormal, CSSValueID::kReverse, CSSValueID::kAlternate,
           CSSValueID::kAlternateReverse}));
}

fuzztest::Domain<std::string> AnyCSSAnimationFillModeValue() {
  return fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      fuzztest::ElementOf<CSSValueID>({CSSValueID::kNone, CSSValueID::kForwards,
                                       CSSValueID::kBackwards,
                                       CSSValueID::kBoth}));
}

fuzztest::Domain<std::string> AnyCSSAnimationPlayStateValue() {
  return fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      fuzztest::ElementOf<CSSValueID>(
          {CSSValueID::kRunning, CSSValueID::kPaused}));
}

fuzztest::Domain<std::string> AnyCSSAnimationTimingFunctionValue() {
  // linear( [ <number> && <percentage>{0,2} ]# )
  auto linear_point = fuzztest::OneOf(
      fuzztest::Map([](double v) { return base::NumberToString(v); },
                    fuzztest::Finite<double>()),
      fuzztest::Map(
          [](double v, int p) {
            return base::StrCat(
                {base::NumberToString(v), " ", base::NumberToString(p), "%"});
          },
          fuzztest::Finite<double>(), fuzztest::InRange(0, 100)),
      fuzztest::Map(
          [](double v, int p1, int p2) {
            return base::StrCat({base::NumberToString(v), " ",
                                 base::NumberToString(p1), "% ",
                                 base::NumberToString(p2), "%"});
          },
          fuzztest::Finite<double>(), fuzztest::InRange(0, 100),
          fuzztest::InRange(0, 100)));
  auto steps_count = fuzztest::InRange(1, 100);
  return fuzztest::OneOf(
      // linear | ease | ease-in | ease-out | ease-in-out
      fuzztest::Map(
          [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
          fuzztest::ElementOf<CSSValueID>(
              {CSSValueID::kLinear, CSSValueID::kEase, CSSValueID::kEaseIn,
               CSSValueID::kEaseOut, CSSValueID::kEaseInOut})),
      // step-start | step-end
      fuzztest::ElementOf<std::string>({"step-start", "step-end"}),
      // steps(<n>)
      fuzztest::Map(
          [](int steps) {
            return base::StrCat({"steps(", base::NumberToString(steps), ")"});
          },
          steps_count),
      // steps(<n>, <step-position>) except jump-none (see below)
      fuzztest::Map(
          [](int steps, const std::string& position) {
            return base::StrCat(
                {"steps(", base::NumberToString(steps), ", ", position, ")"});
          },
          steps_count,
          fuzztest::ElementOf<std::string>(
              {"jump-start", "jump-end", "jump-both", "start", "end"})),
      // jump-none requires at least two steps.
      fuzztest::Map(
          [](int steps) {
            return base::StrCat(
                {"steps(", base::NumberToString(steps), ", jump-none)"});
          },
          fuzztest::InRange(2, 100)),
      // cubic-bezier(<x1>, <y1>, <x2>, <y2>) with x1/x2 in [0, 1]
      fuzztest::Map(
          [](double x1, double y1, double x2, double y2) {
            return base::StrCat({"cubic-bezier(", base::NumberToString(x1), ",",
                                 base::NumberToString(y1), ",",
                                 base::NumberToString(x2), ",",
                                 base::NumberToString(y2), ")"});
          },
          fuzztest::InRange<double>(0.0, 1.0), fuzztest::Finite<double>(),
          fuzztest::InRange<double>(0.0, 1.0), fuzztest::Finite<double>()),
      // linear(<point>[, <point>]...)
      fuzztest::Map(
          [](std::vector<std::string> points) {
            return base::StrCat(
                {"linear(", base::JoinString(points, ", "), ")"});
          },
          fuzztest::VectorOf(linear_point).WithMinSize(2).WithMaxSize(10)));
}

fuzztest::Domain<std::string> AnyCSSAnimationDurationValue() {
  return fuzztest::Map(
      [](int v) { return base::StrCat({base::NumberToString(v), "s"}); },
      fuzztest::InRange(0, 5));
}

fuzztest::Domain<std::string> AnyCSSAnimationDelayValue() {
  return fuzztest::Map(
      [](int v) { return base::StrCat({base::NumberToString(v), "s"}); },
      fuzztest::InRange(-5, 10));
}

fuzztest::Domain<std::string> AnyCSSAnimationIterationCountValue() {
  return fuzztest::OneOf(
      fuzztest::Map(
          [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
          fuzztest::Just(CSSValueID::kInfinite)),
      fuzztest::Map([](int v) { return base::NumberToString(v); },
                    fuzztest::InRange(0, 100)));
}

fuzztest::Domain<std::string> AnyCSSAnimationNameValue() {
  return fuzztest::ElementOf<std::string>(
      {"fuzz-fade", "fuzz-hide", "fuzz-reveal", "fuzz-shrink",
       "fuzz-shrink-to-zero", "fuzz-move", "fuzz-move-offscreen", "fuzz-spin",
       "fuzz-tilt", "fuzz-pulse", "fuzz-recolor", "fuzz-toggle-display",
       "none"});
}

fuzztest::Domain<std::string> AnyCSSScrollMarkerGroupValue() {
  // scroll-marker-group: none | [[before | after] || [links | tabs]]
  auto position = fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      fuzztest::ElementOf<CSSValueID>(
          {CSSValueID::kAfter, CSSValueID::kBefore}));
  auto mode = fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      fuzztest::ElementOf<CSSValueID>({CSSValueID::kTabs, CSSValueID::kLinks}));
  return fuzztest::OneOf(
      fuzztest::Just(std::string(GetCSSValueName(CSSValueID::kNone))), position,
      mode,
      fuzztest::Map(
          [](const std::string& pos, const std::string& m) {
            return base::StrCat({pos, " ", m});
          },
          position, mode));
}

fuzztest::Domain<std::string> AnyCSSScrollTargetGroupValue() {
  return fuzztest::Map(
      CSSEnumToString<EScrollTargetGroup>,
      fuzztest::InRange(0,
                        static_cast<int>(EScrollTargetGroup::kMaxEnumValue)));
}

fuzztest::Domain<std::string> AnyCSSScrollSnapAlignValue() {
  auto single = fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      fuzztest::ElementOf<CSSValueID>({CSSValueID::kNone, CSSValueID::kStart,
                                       CSSValueID::kCenter, CSSValueID::kEnd}));
  return fuzztest::OneOf(
      single,
      fuzztest::Map(
          [](const std::string& block_val, const std::string& inline_val) {
            return base::StrCat({block_val, " ", inline_val});
          },
          single, single));
}

fuzztest::Domain<std::string> AnyCSSScrollSnapTypeValue() {
  auto axis = fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      fuzztest::ElementOf<CSSValueID>({CSSValueID::kX, CSSValueID::kY,
                                       CSSValueID::kBlock, CSSValueID::kInline,
                                       CSSValueID::kBoth}));
  auto strictness = fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      fuzztest::ElementOf<CSSValueID>(
          {CSSValueID::kMandatory, CSSValueID::kProximity}));
  return fuzztest::OneOf(
      fuzztest::Just(std::string(GetCSSValueName(CSSValueID::kNone))),
      fuzztest::Map(
          [](const std::string& ax, const std::string& str) {
            return base::StrCat({ax, " ", str});
          },
          axis, strictness));
}

fuzztest::Domain<std::string> AnyCSSOpacityValue() {
  return fuzztest::Map([](float v) { return base::NumberToString(v); },
                       fuzztest::InRange<float>(0.0f, 1.0f));
}

fuzztest::Domain<std::string> AnyCSSLengthValue() {
  return fuzztest::Map(
      [](float val, const std::string& unit) {
        return base::StrCat({base::NumberToString(val), unit});
      },
      fuzztest::Finite<float>(),
      fuzztest::ElementOf<std::string>({"px", "em", "vh", "vw"}));
}

fuzztest::Domain<std::string> AnyCSSNonNegativeLengthPercentageValue() {
  auto numeric = fuzztest::Map(
      [](float val, const std::string& unit) {
        return base::StrCat({base::NumberToString(val), unit});
      },
      fuzztest::Positive<float>(),
      fuzztest::ElementOf<std::string>({"px", "%", "em", "vh", "vw"}));
  auto calc = fuzztest::Map(
      [](const std::string& expr) {
        return base::StrCat({"calc(", expr, ")"});
      },
      AnyCSSCalcSumValue());
  return fuzztest::OneOf(numeric, calc);
}

fuzztest::Domain<std::string> AnyCSSSizeValue() {
  auto keyword = fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      fuzztest::ElementOf<CSSValueID>(
          {CSSValueID::kAuto, CSSValueID::kFitContent, CSSValueID::kMinContent,
           CSSValueID::kMaxContent, CSSValueID::kStretch}));
  // <calc-size()> = calc-size( <calc-size-basis>, <calc-sum> )
  // <calc-size-basis> = [ <size-keyword> | <calc-size()> | any | <calc-sum> ]
  auto calc_size_basis = fuzztest::OneOf(
      fuzztest::ElementOf<std::string>({"auto", "min-content", "max-content",
                                        "fit-content", "stretch", "any"}),
      AnyCSSCalcSumValue());
  auto calc_size = fuzztest::Map(
      [](const std::string& basis, int delta) {
        return base::StrCat({"calc-size(", basis, ", size + ",
                             base::NumberToString(delta), "px)"});
      },
      calc_size_basis, fuzztest::InRange(0, 10000));
  return fuzztest::OneOf(AnyCSSNonNegativeLengthPercentageValue(), keyword,
                         calc_size);
}

fuzztest::Domain<std::string> AnyCSSMaxSizeValue() {
  auto keyword = fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      fuzztest::ElementOf<CSSValueID>(
          {CSSValueID::kNone, CSSValueID::kFitContent, CSSValueID::kMinContent,
           CSSValueID::kMaxContent, CSSValueID::kStretch}));
  // <calc-size()> = calc-size( <calc-size-basis>, <calc-sum> )
  // <calc-size-basis> = [ <size-keyword> | <calc-size()> | any | <calc-sum> ]
  auto calc_size_basis = fuzztest::OneOf(
      fuzztest::ElementOf<std::string>(
          {"min-content", "max-content", "fit-content", "stretch", "any"}),
      AnyCSSCalcSumValue());
  auto calc_size = fuzztest::Map(
      [](const std::string& basis, int delta) {
        return base::StrCat({"calc-size(", basis, ", size + ",
                             base::NumberToString(delta), "px)"});
      },
      calc_size_basis, fuzztest::InRange(0, 10000));
  return fuzztest::OneOf(AnyCSSNonNegativeLengthPercentageValue(), keyword,
                         calc_size);
}

fuzztest::Domain<std::string> AnyCSSTransformValue() {
  return fuzztest::OneOf(
      fuzztest::Just(std::string(GetCSSValueName(CSSValueID::kNone))),
      AnyCSSTranslateValue(), AnyCSSScaleValue(), AnyCSSRotateValue(),
      AnyCSSSkewValue(), AnyCSSPerspectiveValue(), AnyCSSMatrixValue());
}

fuzztest::Domain<WebAnimationParams> AnyWebAnimationParams() {
  return fuzztest::FlatMap(
      [](CSSPropertyID property) {
        return fuzztest::StructOf<WebAnimationParams>(
            fuzztest::Just(property), AnyPlausibleValueForCSSProperty(property),
            AnyPlausibleValueForCSSProperty(property));
      },
      fuzztest::ElementOf<CSSPropertyID>(
          {CSSPropertyID::kOpacity, CSSPropertyID::kTransform,
           CSSPropertyID::kWidth, CSSPropertyID::kHeight,
           CSSPropertyID::kDisplay, CSSPropertyID::kVisibility,
           CSSPropertyID::kContentVisibility, CSSPropertyID::kColor,
           CSSPropertyID::kBackgroundColor, CSSPropertyID::kMarginTop,
           CSSPropertyID::kPaddingTop}));
}

fuzztest::Domain<std::string> AnyPlausibleValueForCSSProperty(
    CSSPropertyID property) {
  // Basic keyword-valued properties.
  if (property == CSSPropertyID::kDisplay) {
    return AnyCSSDisplayValue();
  }
  if (property == CSSPropertyID::kPosition) {
    return AnyCSSPositionValue();
  }
  if (property == CSSPropertyID::kVisibility) {
    return AnyCSSVisibilityValue();
  }
  if (property == CSSPropertyID::kContentVisibility) {
    return AnyCSSContentVisibilityValue();
  }
  if (property == CSSPropertyID::kOverflowX ||
      property == CSSPropertyID::kOverflowY) {
    return AnyCSSOverflowValue();
  }
  if (property == CSSPropertyID::kTextOrientation) {
    return AnyCSSTextOrientationValue();
  }
  if (property == CSSPropertyID::kFlexDirection) {
    return AnyCSSFlexDirectionValue();
  }
  if (property == CSSPropertyID::kReadingFlow) {
    return AnyCSSReadingFlowValue();
  }
  if (property == CSSPropertyID::kReadingOrder ||
      property == CSSPropertyID::kOrder) {
    return fuzztest::Map([](int v) { return base::NumberToString(v); },
                         fuzztest::Arbitrary<int>());
  }
  if (property == CSSPropertyID::kTextOverflow) {
    return AnyCSSTextOverflowValue();
  }

  // Animation-related properties.
  if (property == CSSPropertyID::kAnimationName) {
    return AnyCSSAnimationNameValue();
  }
  if (property == CSSPropertyID::kAnimationDuration) {
    return AnyCSSAnimationDurationValue();
  }
  if (property == CSSPropertyID::kAnimationDelay) {
    return AnyCSSAnimationDelayValue();
  }
  if (property == CSSPropertyID::kAnimationDirection) {
    return AnyCSSAnimationDirectionValue();
  }
  if (property == CSSPropertyID::kAnimationFillMode) {
    return AnyCSSAnimationFillModeValue();
  }
  if (property == CSSPropertyID::kAnimationIterationCount) {
    return AnyCSSAnimationIterationCountValue();
  }
  if (property == CSSPropertyID::kAnimationPlayState) {
    return AnyCSSAnimationPlayStateValue();
  }
  if (property == CSSPropertyID::kAnimationTimingFunction) {
    return AnyCSSAnimationTimingFunctionValue();
  }

  // Scroll and carousel-related properties.
  if (property == CSSPropertyID::kScrollMarkerGroup) {
    return AnyCSSScrollMarkerGroupValue();
  }
  if (property == CSSPropertyID::kScrollTargetGroup) {
    return AnyCSSScrollTargetGroupValue();
  }
  if (property == CSSPropertyID::kScrollSnapAlign) {
    return AnyCSSScrollSnapAlignValue();
  }
  if (property == CSSPropertyID::kScrollSnapType) {
    return AnyCSSScrollSnapTypeValue();
  }
  if (property == CSSPropertyID::kScrollInitialTarget) {
    return fuzztest::Map(
        CSSEnumToString<EScrollInitialTarget>,
        fuzztest::InRange(
            0, static_cast<int>(EScrollInitialTarget::kMaxEnumValue)));
  }

  // Numeric and geometric properties.
  if (property == CSSPropertyID::kOpacity) {
    return AnyCSSOpacityValue();
  }
  if (property == CSSPropertyID::kTransform) {
    return AnyCSSTransformValue();
  }
  if (property == CSSPropertyID::kWidth || property == CSSPropertyID::kHeight ||
      property == CSSPropertyID::kMinWidth ||
      property == CSSPropertyID::kMinHeight) {
    return AnyCSSSizeValue();
  }
  if (property == CSSPropertyID::kMaxWidth ||
      property == CSSPropertyID::kMaxHeight) {
    return AnyCSSMaxSizeValue();
  }
  if (property == CSSPropertyID::kMarginTop ||
      property == CSSPropertyID::kMarginRight ||
      property == CSSPropertyID::kMarginBottom ||
      property == CSSPropertyID::kMarginLeft) {
    return AnyCSSLengthPercentageValue();
  }
  if (property == CSSPropertyID::kPaddingTop ||
      property == CSSPropertyID::kPaddingRight ||
      property == CSSPropertyID::kPaddingBottom ||
      property == CSSPropertyID::kPaddingLeft) {
    return AnyCSSNonNegativeLengthPercentageValue();
  }

  // Colors.
  if (property == CSSPropertyID::kColor ||
      property == CSSPropertyID::kBackgroundColor ||
      property == CSSPropertyID::kBorderColor ||
      property == CSSPropertyID::kOutlineColor) {
    return AnyColorValue();
  }

  // For properties we haven't specifically handled, use any valid CSS value
  return fuzztest::Map(
      [](CSSValueID val) { return std::string(GetCSSValueName(val)); },
      AnyCSSValue());
}

fuzztest::Domain<std::string> AnyValueForCSSProperty(CSSPropertyID property) {
  return fuzztest::OneOf(AnyPlausibleValueForCSSProperty(property),
                         fuzztest::Arbitrary<std::string>());
}

fuzztest::Domain<std::string> AnyCSSPropertyNameValuePair() {
  return fuzztest::FlatMap(
      [](CSSPropertyID property) {
        return fuzztest::Map(
            [property](const std::string& value) {
              CSSPropertyName prop_name(property);
              const std::string prop_name_str(
                  prop_name.ToAtomicString().Utf8());
              return base::StrCat({prop_name_str, ": ", value, ";"});
            },
            AnyValueForCSSProperty(property));
      },
      AnyCSSProperty());
}

fuzztest::Domain<std::string> AnyCssDeclaration() {
  return fuzztest::Map(
      [](base::span<const std::string> properties) {
        return base::JoinString(properties, " ");
      },
      fuzztest::VectorOf(AnyCSSPropertyNameValuePair()).WithMaxSize(3));
}

}  // namespace blink
