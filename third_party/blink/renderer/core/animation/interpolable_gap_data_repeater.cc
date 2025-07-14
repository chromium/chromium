// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_gap_data_repeater.h"

#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/style/gap_data.h"

namespace blink {

InterpolableGapLengthRepeater* InterpolableGapLengthRepeater::Create(
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
  return MakeGarbageCollected<InterpolableGapLengthRepeater>(values, repeater);
}

void InterpolableGapLengthRepeater::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableGapLengthRepeater& other_interpolable_gap_data_repeater =
      To<InterpolableGapLengthRepeater>(other);
  DCHECK_EQ(values_->length(),
            other_interpolable_gap_data_repeater.values_->length());
  values_->AssertCanInterpolateWith(
      *other_interpolable_gap_data_repeater.values_);
}

bool InterpolableGapLengthRepeater::IsCompatibleWith(
    const InterpolableValue& other) const {
  const InterpolableGapLengthRepeater& other_interpolable_gap_data_repeater =
      To<InterpolableGapLengthRepeater>(other);
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

GapData<int> InterpolableGapLengthRepeater::CreateGapData(
    const CSSToLengthConversionData& conversion_data,
    Length::ValueRange value_range) const {
  WTF::Vector<int> repeated_values;
  repeated_values.ReserveInitialCapacity(values_->length());
  for (wtf_size_t i = 0; i < values_->length(); ++i) {
    const InterpolableLength& length = To<InterpolableLength>(*values_->Get(i));
    repeated_values.push_back(
        length.CreateLength(conversion_data, value_range).IntValue());
  }

  ValueRepeater<int>* repeater = MakeGarbageCollected<ValueRepeater<int>>(
      repeated_values,
      repeater_->IsAutoRepeater()
          ? std::nullopt
          : std::optional<wtf_size_t>(repeater_->RepeatCount()));

  GapData<int> gap_data(repeater);

  return gap_data;
}

}  // namespace blink
