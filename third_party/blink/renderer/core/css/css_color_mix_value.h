// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COLOR_MIX_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COLOR_MIX_VALUE_H_

#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

class CSSLengthResolver;
class CSSPrimitiveValue;

namespace cssvalue {

struct CSSColorMixItem {
  DISALLOW_NEW();

  bool operator==(const CSSColorMixItem& other) const {
    return base::ValuesEquivalent(color, other.color) &&
           base::ValuesEquivalent(percentage, other.percentage);
  }

  CORE_EXPORT void Trace(Visitor*) const;

  Member<const CSSValue> color;
  // nullptr if the percentage was omitted.
  Member<const CSSPrimitiveValue> percentage;
};

}  // namespace cssvalue
}  // namespace blink

// We have to declare the VectorTraits specialization before CSSColorMixValue
// declares its vector below.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::cssvalue::CSSColorMixItem)

namespace blink {
namespace cssvalue {

// This is a class for storing the result of parsing the color-mix function
// before resolving it into a blink::Color. See
// https://www.w3.org/TR/css-color-5/#color-mix
class CORE_EXPORT CSSColorMixValue : public CSSValue {
 public:
  using ItemVector = HeapVector<CSSColorMixItem>;
  using PercentageVector = HeapVector<Member<const CSSPrimitiveValue>, 2>;

  // NormalizedPercentages and its helpers will eventually be shared by other
  // *-mix() functions.
  //
  // https://drafts.csswg.org/css-values-5/#normalize-mix-percentages
  struct NormalizedPercentages {
    Vector<double, 2> percentages;
    double alpha_multiplier = 1.0;
  };

  CSSColorMixValue(ItemVector items,
                   const Color::ColorSpace color_interpolation_space,
                   const Color::HueInterpolationMethod hue_interpolation_method)
      : CSSValue(kColorMixClass),
        items_(std::move(items)),
        color_interpolation_space_(color_interpolation_space),
        hue_interpolation_method_(hue_interpolation_method) {
    CHECK(!items_.empty());
  }

  String CustomCSSText() const;

  void TraceAfterDispatch(blink::Visitor* visitor) const;

  bool Equals(const CSSColorMixValue& other) const;

  const ItemVector& Items() const { return items_; }
  Color::ColorSpace ColorInterpolationSpace() const {
    return color_interpolation_space_;
  }
  Color::HueInterpolationMethod HueInterpolationMethod() const {
    return hue_interpolation_method_;
  }

  // Takes fractions (1 is 100%), as Color::FromColorMix() does.
  static NormalizedPercentages NormalizeMixPercentages(
      base::span<const std::optional<double>> percentages);
  NormalizedPercentages NormalizeMixPercentages(
      const CSSLengthResolver& length_resolver) const;

  // https://www.w3.org/TR/css-color-5/#color-mix-percent-norm
  // For palette-mix(), which needs the mix_amount, i.e. the progress towards
  // the second value, computed by this function.
  static void NormalizePercentages(const CSSPrimitiveValue* percentage1,
                                   const CSSPrimitiveValue* percentage2,
                                   double& mix_amount,
                                   double& alpha_multiplier,
                                   const CSSLengthResolver& length_resolver);

  // https://drafts.csswg.org/css-color-5/#serial-color-mix
  static PercentageVector PercentageValuesForSerialization(
      base::span<const Member<const CSSPrimitiveValue>> percentages);
  // For palette-mix(), which has exactly two percentages.
  static std::pair<const CSSPrimitiveValue*, const CSSPrimitiveValue*>
  PercentageValuesForSerialization(const CSSPrimitiveValue* p1,
                                   const CSSPrimitiveValue* p2);

  bool HasRandomFunctions() const;
  bool IsComputationallyIndependent() const;

 private:
  ItemVector items_;
  const Color::ColorSpace color_interpolation_space_;
  const Color::HueInterpolationMethod hue_interpolation_method_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSColorMixValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsColorMixValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COLOR_MIX_VALUE_H_
