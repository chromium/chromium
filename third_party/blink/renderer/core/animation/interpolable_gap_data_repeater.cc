// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_gap_data_repeater.h"

#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_color.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/style/gap_data.h"

namespace blink {

InterpolableGapColorRepeater* InterpolableGapColorRepeater::Create(
    const ValueRepeater<StyleColor>* repeater,
    const ComputedStyle& style) {
  CHECK(repeater);

  InterpolableList* values =
      MakeGarbageCollected<InterpolableList>(repeater->RepeatedValues().size());
  for (wtf_size_t i = 0; i < repeater->RepeatedValues().size(); ++i) {
    InterpolableValue* result =
        CreateItem(repeater->RepeatedValues()[i], style);
    DCHECK(result);
    values->Set(i, std::move(result));
  }
  return MakeGarbageCollected<InterpolableGapColorRepeater>(values, repeater);
}

void InterpolableGapColorRepeater::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableGapColorRepeater& other_interpolable_gap_data_repeater =
      To<InterpolableGapColorRepeater>(other);
  DCHECK_EQ(values_->length(),
            other_interpolable_gap_data_repeater.values_->length());
  values_->AssertCanInterpolateWith(
      *other_interpolable_gap_data_repeater.values_);
}

bool InterpolableGapColorRepeater::IsCompatibleWith(
    const InterpolableValue& other) const {
  const InterpolableGapColorRepeater& other_interpolable_gap_data_repeater =
      To<InterpolableGapColorRepeater>(other);
  const bool is_auto = repeater_->IsAutoRepeater();

  // Both repeaters must be auto or fixed-count repeaters.
  if (is_auto !=
      other_interpolable_gap_data_repeater.repeater_->IsAutoRepeater()) {
    return false;
  }

  if (is_auto) {
    return values_->length() ==
           other_interpolable_gap_data_repeater.values_->length();
  }

  return values_->length() ==
             other_interpolable_gap_data_repeater.values_->length() &&
         repeater_->RepeatCount() ==
             other_interpolable_gap_data_repeater.repeater_->RepeatCount();
}

GapData<StyleColor> InterpolableGapColorRepeater::CreateGapData(
    StyleResolverState& state) const {
  HeapVector<StyleColor> repeated_values;
  repeated_values.ReserveInitialCapacity(values_->length());
  for (wtf_size_t i = 0; i < values_->length(); ++i) {
    const InterpolableColor& interpolable_color =
        To<InterpolableColor>(*values_->Get(i));
    repeated_values.push_back(
        StyleColor(CSSColorInterpolationType::ResolveInterpolableColor(
            interpolable_color, state,
            /*is_visited=*/false,
            /*is_text_decoration=*/false)));
  }

  ValueRepeater<StyleColor>* repeater =
      MakeGarbageCollected<ValueRepeater<StyleColor>>(
          repeated_values,
          repeater_->IsAutoRepeater()
              ? std::nullopt
              : std::optional<wtf_size_t>(repeater_->RepeatCount()));

  GapData<StyleColor> gap_data(repeater);

  return gap_data;
}

InterpolableValue* InterpolableGapColorRepeater::CreateItem(
    const StyleColor& value,
    const ComputedStyle& style) {
  return CSSColorInterpolationType::CreateBaseInterpolableColor(
      value, style.UsedColorScheme(), /*color_provider=*/nullptr);
}

void InterpolableGapColorRepeater::Composite(
    const InterpolableGapColorRepeater& other,
    double fraction) {
  CHECK(IsCompatibleWith(other));

  for (wtf_size_t i = 0; i < values_->length(); ++i) {
    auto& color = To<InterpolableColor>(*values_->GetMutable(i));
    auto& other_color = To<InterpolableColor>(*other.values_->GetMutable(i));
    color.Composite(other_color, fraction);
  }
}

}  // namespace blink
