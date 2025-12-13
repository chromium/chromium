// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_gap_data_auto_repeater.h"

#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_color.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/style/gap_data.h"

namespace blink {

InterpolableGapLengthAutoRepeater* InterpolableGapLengthAutoRepeater::Create(
    const ValueRepeater<int>* repeater,
    const CSSProperty& property,
    float zoom) {
  CHECK(repeater);

  InterpolableList* values =
      MakeGarbageCollected<InterpolableList>(repeater->RepeatedValues().size());
  for (wtf_size_t i = 0; i < repeater->RepeatedValues().size(); ++i) {
    InterpolableValue* result =
        CreateItem(repeater->RepeatedValues()[i], property, zoom);
    DCHECK(result);
    values->Set(i, std::move(result));
  }
  return MakeGarbageCollected<InterpolableGapLengthAutoRepeater>(values,
                                                                 repeater);
}

void InterpolableGapLengthAutoRepeater::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableGapLengthAutoRepeater&
      other_interpolable_gap_data_auto_repeater =
          To<InterpolableGapLengthAutoRepeater>(other);
  DCHECK_EQ(values_->length(),
            other_interpolable_gap_data_auto_repeater.values_->length());
  values_->AssertCanInterpolateWith(
      *other_interpolable_gap_data_auto_repeater.values_);
}

bool InterpolableGapLengthAutoRepeater::IsCompatibleWith(
    const InterpolableValue& other) const {
  const InterpolableGapLengthAutoRepeater&
      other_interpolable_gap_data_auto_repeater =
          To<InterpolableGapLengthAutoRepeater>(other);
  const bool is_auto = repeater_->IsAutoRepeater();

  // Both repeaters must be auto. At this point, they should both be auto
  // repeaters.
  CHECK_EQ(
      is_auto,
      other_interpolable_gap_data_auto_repeater.repeater_->IsAutoRepeater());
  return true;
}

GapData<int> InterpolableGapLengthAutoRepeater::CreateGapData(
    const CSSToLengthConversionData& conversion_data,
    Length::ValueRange value_range) const {
  Vector<int> repeated_values;
  repeated_values.ReserveInitialCapacity(values_->length());
  for (wtf_size_t i = 0; i < values_->length(); ++i) {
    const InterpolableLength& length = To<InterpolableLength>(*values_->Get(i));
    repeated_values.push_back(
        length.CreateLength(conversion_data, value_range).IntValue());
  }

  CHECK(repeater_->IsAutoRepeater());
  ValueRepeater<int>* repeater =
      MakeGarbageCollected<ValueRepeater<int>>(repeated_values, std::nullopt);

  GapData<int> gap_data(repeater);

  return gap_data;
}

void InterpolableGapLengthAutoRepeater::Composite(
    const InterpolableGapLengthAutoRepeater& other,
    double fraction) {
  CHECK(IsCompatibleWith(other));
  for (wtf_size_t i = 0; i < values_->length(); ++i) {
    auto& a = To<InterpolableLength>(*values_->GetMutable(i));
    auto& b = To<InterpolableLength>(*other.values_->Get(i));
    a.ScaleAndAdd(fraction, b);
  }
}

InterpolableGapColorAutoRepeater* InterpolableGapColorAutoRepeater::Create(
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
  return MakeGarbageCollected<InterpolableGapColorAutoRepeater>(values,
                                                                repeater);
}

void InterpolableGapColorAutoRepeater::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableGapColorAutoRepeater&
      other_interpolable_gap_data_auto_repeater =
          To<InterpolableGapColorAutoRepeater>(other);
  DCHECK_EQ(values_->length(),
            other_interpolable_gap_data_auto_repeater.values_->length());
  values_->AssertCanInterpolateWith(
      *other_interpolable_gap_data_auto_repeater.values_);
}

bool InterpolableGapColorAutoRepeater::IsCompatibleWith(
    const InterpolableValue& other) const {
  const InterpolableGapColorAutoRepeater&
      other_interpolable_gap_data_auto_repeater =
          To<InterpolableGapColorAutoRepeater>(other);
  const bool is_auto = repeater_->IsAutoRepeater();

  // Both repeaters must be auto. At this point, they should both be auto
  // repeaters.
  CHECK_EQ(
      is_auto,
      other_interpolable_gap_data_auto_repeater.repeater_->IsAutoRepeater());
  return true;
}

GapData<StyleColor> InterpolableGapColorAutoRepeater::CreateGapData(
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

  CHECK(repeater_->IsAutoRepeater());
  ValueRepeater<StyleColor>* repeater =
      MakeGarbageCollected<ValueRepeater<StyleColor>>(repeated_values,
                                                      std::nullopt);

  GapData<StyleColor> gap_data(repeater);

  return gap_data;
}

InterpolableValue* InterpolableGapColorAutoRepeater::CreateItem(
    const StyleColor& value,
    const ComputedStyle& style) {
  return CSSColorInterpolationType::CreateBaseInterpolableColor(
      value, style.UsedColorScheme(), /*color_provider=*/nullptr);
}

void InterpolableGapColorAutoRepeater::Composite(
    const InterpolableGapColorAutoRepeater& other,
    double fraction) {
  CHECK(IsCompatibleWith(other));

  for (wtf_size_t i = 0; i < values_->length(); ++i) {
    auto& color = To<InterpolableColor>(*values_->GetMutable(i));
    auto& other_color = To<InterpolableColor>(*other.values_->GetMutable(i));
    color.Composite(other_color, fraction);
  }
}

}  // namespace blink
