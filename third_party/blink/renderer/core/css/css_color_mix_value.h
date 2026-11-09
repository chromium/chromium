// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COLOR_MIX_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COLOR_MIX_VALUE_H_

#include <utility>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
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

  // `items` must have exactly two items for now.
  CSSColorMixValue(ItemVector items,
                   const Color::ColorSpace color_interpolation_space,
                   const Color::HueInterpolationMethod hue_interpolation_method)
      : CSSValue(kColorMixClass),
        items_(std::move(items)),
        color_interpolation_space_(color_interpolation_space),
        hue_interpolation_method_(hue_interpolation_method) {
    CHECK_EQ(items_.size(), 2u);
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

  // https://www.w3.org/TR/css-color-5/#color-mix-percent-norm
  static void NormalizePercentages(const CSSPrimitiveValue* percentage1,
                                   const CSSPrimitiveValue* percentage2,
                                   double& mix_amount,
                                   double& alpha_multiplier,
                                   const CSSLengthResolver& length_resolver);
  void NormalizePercentages(double& mix_amount,
                            double& alpha_multiplier,
                            const CSSLengthResolver& length_resolver) const {
    NormalizePercentages(items_[0].percentage, items_[1].percentage, mix_amount,
                         alpha_multiplier, length_resolver);
  }

  // https://drafts.csswg.org/css-color-5/#serial-color-mix
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
