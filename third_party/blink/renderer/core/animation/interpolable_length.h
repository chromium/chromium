// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_LENGTH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_LENGTH_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolation_value.h"
#include "third_party/blink/renderer/core/animation/pairwise_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSToLengthConversionData;
class CSSMathExpressionNode;

class CORE_EXPORT InterpolableLength final : public InterpolableValue {
 public:
  ~InterpolableLength() final {}
  InterpolableLength(CSSLengthArray&& length_array);
  explicit InterpolableLength(const CSSMathExpressionNode& expression);

  static std::unique_ptr<InterpolableLength> CreatePixels(double pixels);
  static std::unique_ptr<InterpolableLength> CreatePercent(double pixels);
  static std::unique_ptr<InterpolableLength> CreateNeutral();

  static std::unique_ptr<InterpolableLength> MaybeConvertCSSValue(
      const CSSValue& value);
  static std::unique_ptr<InterpolableLength> MaybeConvertLength(
      const Length& length,
      float zoom);

  static PairwiseInterpolationValue MergeSingles(
      std::unique_ptr<InterpolableValue> start,
      std::unique_ptr<InterpolableValue> end);

  Length CreateLength(const CSSToLengthConversionData& conversion_data,
                      ValueRange range) const;

  // Unlike CreateLength() this preserves all specified unit types via calc()
  // expressions.
  const CSSPrimitiveValue* CreateCSSValue(ValueRange range) const;

  void SetHasPercentage();
  bool HasPercentage() const;
  void SubtractFromOneHundredPercent();

  std::unique_ptr<InterpolableLength> Clone() const {
    return std::unique_ptr<InterpolableLength>(RawClone());
  }
  std::unique_ptr<InterpolableLength> CloneAndZero() const {
    return std::unique_ptr<InterpolableLength>(RawCloneAndZero());
  }

  // InterpolableValue:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;
  bool IsLength() const final { return true; }
  bool Equals(const InterpolableValue& other) const final {
    NOTREACHED();
    return false;
  }
  void Scale(double scale) final;
  void Add(const InterpolableValue& other) final;
  // We override this to avoid two passes in the case of LengthArrays.
  void ScaleAndAdd(double scale, const InterpolableValue& other) final;
  void AssertCanInterpolateWith(const InterpolableValue& other) const final;

 private:
  InterpolableLength* RawClone() const final;
  InterpolableLength* RawCloneAndZero() const final {
    return new InterpolableLength(CSSLengthArray());
  }

  bool IsLengthArray() const { return type_ == Type::kLengthArray; }
  bool IsExpression() const { return type_ == Type::kExpression; }

  void SetLengthArray(CSSLengthArray&& length_array);
  void SetExpression(const CSSMathExpressionNode& expression);
  const CSSMathExpressionNode& AsExpression() const;

  enum class Type { kLengthArray, kExpression };
  Type type_;
  CSSLengthArray length_array_;
  Persistent<const CSSMathExpressionNode> expression_;
};

template <>
struct DowncastTraits<InterpolableLength> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsLength();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_LENGTH_H_
