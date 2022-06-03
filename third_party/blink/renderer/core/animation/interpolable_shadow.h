// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_SHADOW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_SHADOW_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/animation/interpolation_value.h"
#include "third_party/blink/renderer/core/animation/pairwise_interpolation_value.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSValue;
class StyleResolverState;
class UnderlyingValue;

// Represents a CSS shadow value (such as [0] or [1]), converted into a form
// that can be interpolated from/to.
//
// [0]: https://drafts.csswg.org/css-backgrounds-3/#typedef-shadow
// [1]: https://drafts.fxtf.org/filter-effects/#funcdef-filter-drop-shadow
class InterpolableShadow : public InterpolableValue {
 public:
  InterpolableShadow(std::unique_ptr<InterpolableLength> x,
                     std::unique_ptr<InterpolableLength> y,
                     std::unique_ptr<InterpolableLength> blur,
                     std::unique_ptr<InterpolableLength> spread,
                     std::unique_ptr<InterpolableValue> color,
                     ShadowStyle);

  static std::unique_ptr<InterpolableShadow> Create(const ShadowData&,
                                                    double zoom);
  static std::unique_ptr<InterpolableShadow> CreateNeutral();

  static std::unique_ptr<InterpolableShadow> MaybeConvertCSSValue(
      const CSSValue&);

  // Helpers for CSSListInterpolationFunctions.
  static PairwiseInterpolationValue MaybeMergeSingles(
      std::unique_ptr<InterpolableValue> start,
      std::unique_ptr<InterpolableValue> end);
  static bool CompatibleForCompositing(const InterpolableValue*,
                                       const InterpolableValue*);
  static void Composite(UnderlyingValue&,
                        double underlying_fraction,
                        const InterpolableValue&,
                        const NonInterpolableValue*);
  ShadowStyle GetShadowStyle() const { return shadow_style_; }
  // Convert this InterpolableShadow back into a ShadowData class, usually to be
  // applied to the style after interpolating it.
  ShadowData CreateShadowData(const StyleResolverState&) const;

  // InterpolableValue implementation:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;
  bool IsShadow() const final { return true; }
  bool Equals(const InterpolableValue& other) const final {
    NOTREACHED();
    return false;
  }
  void Scale(double scale) final;
  void Add(const InterpolableValue& other) final;
  void AssertCanInterpolateWith(const InterpolableValue& other) const final;

 private:
  InterpolableShadow* RawClone() const final;
  InterpolableShadow* RawCloneAndZero() const final;

  // The interpolable components of a shadow. These should all be non-null.
  std::unique_ptr<InterpolableLength> x_;
  std::unique_ptr<InterpolableLength> y_;
  std::unique_ptr<InterpolableLength> blur_;
  std::unique_ptr<InterpolableLength> spread_;
  std::unique_ptr<InterpolableValue> color_;

  ShadowStyle shadow_style_;
};

template <>
struct DowncastTraits<InterpolableShadow> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsShadow();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_SHADOW_H_
