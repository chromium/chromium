// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_value_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/svg/properties/svg_animated_property.h"

namespace blink {

class SVGValueNonInterpolableValue : public NonInterpolableValue {
 public:
  ~SVGValueNonInterpolableValue() override = default;

  static scoped_refptr<SVGValueNonInterpolableValue> Create(
      SVGPropertyBase* svg_value) {
    return base::AdoptRef(new SVGValueNonInterpolableValue(svg_value));
  }

  SVGPropertyBase* SvgValue() const { return svg_value_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  SVGValueNonInterpolableValue(SVGPropertyBase* svg_value)
      : svg_value_(svg_value) {}

  Persistent<SVGPropertyBase> svg_value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGValueNonInterpolableValue);
template <>
struct DowncastTraits<SVGValueNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == SVGValueNonInterpolableValue::static_type_;
  }
};

InterpolationValue SVGValueInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& value) const {
  SVGPropertyBase* referenced_value =
      const_cast<SVGPropertyBase*>(&value);  // Take ref.
  return InterpolationValue(
      std::make_unique<InterpolableList>(0),
      SVGValueNonInterpolableValue::Create(referenced_value));
}

SVGPropertyBase* SVGValueInterpolationType::AppliedSVGValue(
    const InterpolableValue&,
    const NonInterpolableValue* non_interpolable_value) const {
  return To<SVGValueNonInterpolableValue>(*non_interpolable_value).SvgValue();
}

}  // namespace blink
