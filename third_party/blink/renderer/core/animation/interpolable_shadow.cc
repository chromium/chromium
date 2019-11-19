// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_shadow.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/underlying_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"

namespace blink {
namespace {
std::unique_ptr<InterpolableLength> MaybeConvertLength(
    const CSSPrimitiveValue* value) {
  if (value)
    return InterpolableLength::MaybeConvertCSSValue(*value);
  return InterpolableLength::CreatePixels(0);
}

std::unique_ptr<InterpolableValue> MaybeConvertColor(const CSSValue* value) {
  if (value)
    return CSSColorInterpolationType::MaybeCreateInterpolableColor(*value);
  return CSSColorInterpolationType::CreateInterpolableColor(
      StyleColor::CurrentColor());
}
}  // namespace

InterpolableShadow::InterpolableShadow(
    std::unique_ptr<InterpolableLength> x,
    std::unique_ptr<InterpolableLength> y,
    std::unique_ptr<InterpolableLength> blur,
    std::unique_ptr<InterpolableLength> spread,
    std::unique_ptr<InterpolableValue> color,
    ShadowStyle shadow_style)
    : x_(std::move(x)),
      y_(std::move(y)),
      blur_(std::move(blur)),
      spread_(std::move(spread)),
      color_(std::move(color)),
      shadow_style_(shadow_style) {
  DCHECK(x_);
  DCHECK(y_);
  DCHECK(blur_);
  DCHECK(spread_);
  DCHECK(color_);
}

// static
std::unique_ptr<InterpolableShadow> InterpolableShadow::Create(
    const ShadowData& shadow_data,
    double zoom) {
  return std::make_unique<InterpolableShadow>(
      InterpolableLength::CreatePixels(shadow_data.X() / zoom),
      InterpolableLength::CreatePixels(shadow_data.Y() / zoom),
      InterpolableLength::CreatePixels(shadow_data.Blur() / zoom),
      InterpolableLength::CreatePixels(shadow_data.Spread() / zoom),
      CSSColorInterpolationType::CreateInterpolableColor(
          shadow_data.GetColor()),
      shadow_data.Style());
}

// static
std::unique_ptr<InterpolableShadow> InterpolableShadow::CreateNeutral() {
  return Create(ShadowData::NeutralValue(), 1);
}

// static
std::unique_ptr<InterpolableShadow> InterpolableShadow::MaybeConvertCSSValue(
    const CSSValue& value) {
  const auto* shadow = DynamicTo<CSSShadowValue>(value);
  if (!shadow)
    return nullptr;

  ShadowStyle shadow_style = kNormal;
  if (shadow->style) {
    if (shadow->style->GetValueID() != CSSValueID::kInset)
      return nullptr;
    shadow_style = kInset;
  }

  std::unique_ptr<InterpolableLength> x = MaybeConvertLength(shadow->x.Get());
  std::unique_ptr<InterpolableLength> y = MaybeConvertLength(shadow->y.Get());
  std::unique_ptr<InterpolableLength> blur =
      MaybeConvertLength(shadow->blur.Get());
  std::unique_ptr<InterpolableLength> spread =
      MaybeConvertLength(shadow->spread.Get());
  std::unique_ptr<InterpolableValue> color = MaybeConvertColor(shadow->color);

  // If any of the conversations failed, we can't represent this CSSValue.
  if (!x || !y || !blur || !spread || !color)
    return nullptr;

  return std::make_unique<InterpolableShadow>(
      std::move(x), std::move(y), std::move(blur), std::move(spread),
      std::move(color), shadow_style);
}

// static
PairwiseInterpolationValue InterpolableShadow::MaybeMergeSingles(
    std::unique_ptr<InterpolableValue> start,
    std::unique_ptr<InterpolableValue> end) {
  if (To<InterpolableShadow>(start.get())->shadow_style_ !=
      To<InterpolableShadow>(end.get())->shadow_style_)
    return nullptr;
  return PairwiseInterpolationValue(std::move(start), std::move(end));
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
  Length shadow_x = x_->CreateLength(conversion_data, kValueRangeAll);
  Length shadow_y = y_->CreateLength(conversion_data, kValueRangeAll);
  Length shadow_blur =
      blur_->CreateLength(conversion_data, kValueRangeNonNegative);
  Length shadow_spread = spread_->CreateLength(conversion_data, kValueRangeAll);
  DCHECK(shadow_x.IsFixed() && shadow_y.IsFixed() && shadow_blur.IsFixed() &&
         shadow_spread.IsFixed());
  return ShadowData(
      FloatPoint(shadow_x.Value(), shadow_y.Value()), shadow_blur.Value(),
      shadow_spread.Value(), shadow_style_,
      CSSColorInterpolationType::ResolveInterpolableColor(*color_, state));
}

InterpolableShadow* InterpolableShadow::RawClone() const {
  return new InterpolableShadow(x_->Clone(), y_->Clone(), blur_->Clone(),
                                spread_->Clone(), color_->Clone(),
                                shadow_style_);
}

InterpolableShadow* InterpolableShadow::RawCloneAndZero() const {
  return new InterpolableShadow(x_->CloneAndZero(), y_->CloneAndZero(),
                                blur_->CloneAndZero(), spread_->CloneAndZero(),
                                color_->CloneAndZero(), shadow_style_);
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
