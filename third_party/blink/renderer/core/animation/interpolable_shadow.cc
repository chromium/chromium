// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_shadow.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_color.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/animation/underlying_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {
namespace {
InterpolableLength* MaybeConvertLength(const CSSPrimitiveValue* value) {
  if (value) {
    return InterpolableLength::MaybeConvertCSSValue(*value);
  }
  return InterpolableLength::CreatePixels(0);
}

InterpolableColor* MaybeConvertColor(const CSSValue* value,
                                     mojom::blink::ColorScheme color_scheme,
                                     const ui::ColorProvider* color_provider) {
  if (value) {
    return CSSColorInterpolationType::MaybeCreateInterpolableColor(
        *value, color_scheme, color_provider);
  }
  return CSSColorInterpolationType::CreateInterpolableColor(
      StyleColor::CurrentColor(), color_scheme, color_provider);
}
}  // namespace

InterpolableShadow::InterpolableShadow(InterpolableLength* x,
                                       InterpolableLength* y,
                                       InterpolableLength* blur,
                                       InterpolableLength* spread,
                                       InterpolableColor* color,
                                       ShadowStyle shadow_style)
    : x_(x),
      y_(y),
      blur_(blur),
      spread_(spread),
      color_(color),
      shadow_style_(shadow_style) {
  DCHECK(x_);
  DCHECK(y_);
  DCHECK(blur_);
  DCHECK(spread_);
  DCHECK(color_);
}

// static
InterpolableShadow* InterpolableShadow::Create(
    const ShadowData& shadow_data,
    double zoom,
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider) {
  return MakeGarbageCollected<InterpolableShadow>(
      InterpolableLength::CreatePixels(shadow_data.X() / zoom),
      InterpolableLength::CreatePixels(shadow_data.Y() / zoom),
      InterpolableLength::CreatePixels(shadow_data.Blur() / zoom),
      InterpolableLength::CreatePixels(shadow_data.Spread() / zoom),
      CSSColorInterpolationType::CreateInterpolableColor(
          shadow_data.GetColor(), color_scheme, color_provider),
      shadow_data.Style());
}

// static
InterpolableShadow* InterpolableShadow::CreateNeutral() {
  // It is okay to pass in `kLight` for `color_scheme` and nullptr for
  // `color_provider` because the neutral color value for shadow data is
  // guaranteed not to be a system color.
  return Create(ShadowData::NeutralValue(), 1,
                /*color_scheme=*/mojom::blink::ColorScheme::kLight,
                /*color_provider=*/nullptr);
}

// static
InterpolableShadow* InterpolableShadow::MaybeConvertCSSValue(
    const CSSValue& value,
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider) {
  const auto* shadow = DynamicTo<CSSShadowValue>(value);
  if (!shadow) {
    return nullptr;
  }

  ShadowStyle shadow_style = ShadowStyle::kNormal;
  if (shadow->style) {
    if (shadow->style->GetValueID() != CSSValueID::kInset) {
      return nullptr;
    }
    shadow_style = ShadowStyle::kInset;
  }

  InterpolableLength* x = MaybeConvertLength(shadow->x.Get());
  InterpolableLength* y = MaybeConvertLength(shadow->y.Get());
  InterpolableLength* blur = MaybeConvertLength(shadow->blur.Get());
  InterpolableLength* spread = MaybeConvertLength(shadow->spread.Get());
  InterpolableColor* color =
      MaybeConvertColor(shadow->color, color_scheme, color_provider);

  // If any of the conversations failed, we can't represent this CSSValue.
  if (!x || !y || !blur || !spread || !color) {
    return nullptr;
  }

  return MakeGarbageCollected<InterpolableShadow>(x, y, blur, spread, color,
                                                  shadow_style);
}

// static
PairwiseInterpolationValue InterpolableShadow::MaybeMergeSingles(
    InterpolableValue* start,
    InterpolableValue* end) {
  InterpolableShadow* start_shadow = To<InterpolableShadow>(start);
  InterpolableShadow* end_shadow = To<InterpolableShadow>(end);

  if (start_shadow->shadow_style_ != end_shadow->shadow_style_) {
    return nullptr;
  }

  // Confirm that both colors are in the same colorspace and adjust if
  // necessary.
  InterpolableColor::SetupColorInterpolationSpaces(*start_shadow->color_,
                                                   *end_shadow->color_);

  return PairwiseInterpolationValue(start, end);
}

//  static
bool InterpolableShadow::CompatibleForCompositing(const InterpolableValue* from,
                                                  const InterpolableValue* to) {
  return To<InterpolableShadow>(from)->shadow_style_ ==
         To<InterpolableShadow>(to)->shadow_style_;
}

// static
void InterpolableShadow::Composite(UnderlyingValue& underlying_value,
                                   double underlying_fraction,
                                   const InterpolableValue& interpolable_value,
                                   const NonInterpolableValue*) {
  InterpolableShadow& underlying_shadow =
      To<InterpolableShadow>(underlying_value.MutableInterpolableValue());
  const InterpolableShadow& interpolable_shadow =
      To<InterpolableShadow>(interpolable_value);
  DCHECK_EQ(underlying_shadow.shadow_style_, interpolable_shadow.shadow_style_);
  underlying_shadow.ScaleAndAdd(underlying_fraction, interpolable_shadow);
}

ShadowData InterpolableShadow::CreateShadowData(
    const StyleResolverState& state) const {
  const CSSToLengthConversionData& conversion_data =
      state.CssToLengthConversionData();
  Length shadow_x = x_->CreateLength(conversion_data, Length::ValueRange::kAll);
  Length shadow_y = y_->CreateLength(conversion_data, Length::ValueRange::kAll);
  Length shadow_blur =
      blur_->CreateLength(conversion_data, Length::ValueRange::kNonNegative);
  Length shadow_spread =
      spread_->CreateLength(conversion_data, Length::ValueRange::kAll);
  DCHECK(shadow_x.IsFixed());
  DCHECK(shadow_y.IsFixed());
  DCHECK(shadow_blur.IsFixed());
  DCHECK(shadow_spread.IsFixed());
  return ShadowData(
      gfx::Vector2dF(shadow_x.Value(), shadow_y.Value()), shadow_blur.Value(),
      shadow_spread.Value(), shadow_style_,
      StyleColor(
          CSSColorInterpolationType::ResolveInterpolableColor(*color_, state)));
}

InterpolableShadow* InterpolableShadow::RawClone() const {
  return MakeGarbageCollected<InterpolableShadow>(
      x_->Clone(), y_->Clone(), blur_->Clone(), spread_->Clone(),
      color_->Clone(), shadow_style_);
}

InterpolableShadow* InterpolableShadow::RawCloneAndZero() const {
  return MakeGarbageCollected<InterpolableShadow>(
      x_->CloneAndZero(), y_->CloneAndZero(), blur_->CloneAndZero(),
      spread_->CloneAndZero(), color_->CloneAndZero(), shadow_style_);
}

void InterpolableShadow::Scale(double scale) {
  x_->Scale(scale);
  y_->Scale(scale);
  blur_->Scale(scale);
  spread_->Scale(scale);
  color_->Scale(scale);
}

void InterpolableShadow::Add(const InterpolableValue& other) {
  const InterpolableShadow& other_shadow = To<InterpolableShadow>(other);
  x_->Add(*other_shadow.x_);
  y_->Add(*other_shadow.y_);
  blur_->Add(*other_shadow.blur_);
  spread_->Add(*other_shadow.spread_);
  color_->Add(*other_shadow.color_);
}

void InterpolableShadow::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableShadow& other_shadow = To<InterpolableShadow>(other);
  DCHECK_EQ(shadow_style_, other_shadow.shadow_style_);
  x_->AssertCanInterpolateWith(*other_shadow.x_);
  y_->AssertCanInterpolateWith(*other_shadow.y_);
  blur_->AssertCanInterpolateWith(*other_shadow.blur_);
  spread_->AssertCanInterpolateWith(*other_shadow.spread_);
  color_->AssertCanInterpolateWith(*other_shadow.color_);
}

void InterpolableShadow::Interpolate(const InterpolableValue& to,
                                     const double progress,
                                     InterpolableValue& result) const {
  const InterpolableShadow& to_shadow = To<InterpolableShadow>(to);
  InterpolableShadow& result_shadow = To<InterpolableShadow>(result);

  x_->Interpolate(*to_shadow.x_, progress, *result_shadow.x_);
  y_->Interpolate(*to_shadow.y_, progress, *result_shadow.y_);
  blur_->Interpolate(*to_shadow.blur_, progress, *result_shadow.blur_);
  spread_->Interpolate(*to_shadow.spread_, progress, *result_shadow.spread_);
  color_->Interpolate(*to_shadow.color_, progress, *result_shadow.color_);
}

}  // namespace blink
