// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_COLOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_COLOR_H_

#include <memory>
#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

// InterpolableColors are created and manipulated by CSSColorInterpolationType.
// They store the three params and alpha from a blink::Color for interpolation,
// along with its color space. It is important that two colors are in the same
// color space when interpolating or the results will be incorrect. This is
// verified and adjusted in CSSColorInterpolationType::MaybeMergeSingles.
class CORE_EXPORT InterpolableColor : public InterpolableValue {
 public:
  InterpolableColor();

  // Certain color keywords cannot be eagerly evaluated at specified value time.
  // For these keywords we store a separate entry in a list here, interpolate
  // that and evaluate the actual value of the color in
  // CSSColorInterpolationType::ResolveInterpolableColor. These entries
  // correspond to the color keywords that require this behavior.
  enum class ColorKeyword : unsigned {
    kCurrentcolor,
    kWebkitActivelink,
    kWebkitLink,
    kQuirkInherit,
    kCount,
  };
  constexpr static unsigned kColorKeywordCount =
      static_cast<int>(ColorKeyword::kCount);

  static std::unique_ptr<InterpolableColor> Create(Color color);
  static std::unique_ptr<InterpolableColor> Create(ColorKeyword color_keyword);
  static std::unique_ptr<InterpolableColor> Create(CSSValueID keyword);

  Color GetColor() const;
  bool IsColor() const final { return true; }

  // Mutates the underlying colorspaces and values of "to" and "from" so that
  // they match for interpolation.
  static void SetupColorInterpolationSpaces(InterpolableColor& to,
                                            InterpolableColor& from);

  void Scale(double scale) final;
  void Add(const InterpolableValue& other) final;
  void AssertCanInterpolateWith(const InterpolableValue& other) const final;
  bool Equals(const InterpolableValue& other) const final {
    NOTREACHED();
    return false;
  }

  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;

  bool IsKeywordColor() const;

  double Param0() const { return param0_.Value(); }
  double Param1() const { return param1_.Value(); }
  double Param2() const { return param2_.Value(); }
  double Alpha() const { return alpha_.Value(); }
  Color::ColorSpace ColorSpace() const { return color_space_; }

  double GetColorFraction(ColorKeyword keyword) const {
    int keyword_index = static_cast<int>(keyword);
    return color_keyword_fractions_.Get(keyword_index).Value();
  }

  std::unique_ptr<InterpolableColor> Clone() const {
    return std::unique_ptr<InterpolableColor>(RawClone());
  }

  std::unique_ptr<InterpolableColor> CloneAndZero() const {
    return std::unique_ptr<InterpolableColor>(RawCloneAndZero());
  }

  void Composite(const InterpolableColor& other, double fraction);

 private:
  using InterpolableNumberList =
      StaticInterpolableList<InterpolableNumber, kColorKeywordCount>;

  InterpolableColor(InterpolableNumber param0,
                    InterpolableNumber param1,
                    InterpolableNumber param2,
                    InterpolableNumber alpha,
                    InterpolableNumberList color_keyword_fractions,
                    Color::ColorSpace color_space);

  void ConvertToColorSpace(Color::ColorSpace color_space);
  InterpolableColor* RawClone() const final;
  InterpolableColor* RawCloneAndZero() const final;

  // All color params are stored premultiplied by alpha.
  // https://csswg.sesse.net/css-color-4/#interpolation-space
  InterpolableNumber param0_;
  InterpolableNumber param1_;
  InterpolableNumber param2_;
  InterpolableNumber alpha_;

  InterpolableNumberList color_keyword_fractions_;

  Color::ColorSpace color_space_ = Color::ColorSpace::kNone;
};

template <>
struct DowncastTraits<InterpolableColor> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsColor();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_COLOR_H_
