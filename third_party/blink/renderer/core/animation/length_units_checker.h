// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_LENGTH_UNITS_CHECKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_LENGTH_UNITS_CHECKER_H_

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"

namespace blink {

class LengthUnitsChecker : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<LengthUnitsChecker> MaybeCreate(
      const CSSPrimitiveValue::LengthTypeFlags& length_types,
      const StyleResolverState& state) {
    bool create = false;
    wtf_size_t last_index = 0;
    CSSLengthArray length_array;
    for (wtf_size_t i = 0; i < length_types.size(); ++i) {
      if (i == CSSPrimitiveValue::kUnitTypePercentage || !length_types[i])
        continue;
      length_array.values[i] = LengthUnit(i, state.CssToLengthConversionData());
      length_array.type_flags.set(i);
      create = true;
      last_index = i;
    }
    if (!create)
      return nullptr;
    return base::WrapUnique(
        new LengthUnitsChecker(std::move(length_array), last_index));
  }

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    for (wtf_size_t i = 0; i <= last_index_; i++) {
      if (i == CSSPrimitiveValue::kUnitTypePercentage ||
          !length_array_.type_flags[i])
        continue;
      if (length_array_.values[i] !=
          LengthUnit(i, state.CssToLengthConversionData()))
        return false;
    }
    return true;
  }

  static double LengthUnit(wtf_size_t length_unit_type,
                           const CSSToLengthConversionData& conversion_data) {
    return conversion_data.ZoomedComputedPixels(
        1,
        CSSPrimitiveValue::LengthUnitTypeToUnitType(
            static_cast<CSSPrimitiveValue::LengthUnitType>(length_unit_type)));
  }

 private:
  LengthUnitsChecker(CSSPrimitiveValue::CSSLengthArray&& length_array,
                     wtf_size_t last_index)
      : length_array_(std::move(length_array)), last_index_(last_index) {}

  const CSSLengthArray length_array_;
  const wtf_size_t last_index_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_LENGTH_UNITS_CHECKER_H_
