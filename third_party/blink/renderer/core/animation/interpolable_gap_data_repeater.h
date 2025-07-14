// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_GAP_DATA_REPEATER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_GAP_DATA_REPEATER_H_

#include <memory>

#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"

namespace blink {

class CSSProperty;
class StyleColor;

// This class is used to interpolate a `GapData` that is a value repeater.
// Essentially, we represent the repeater by keeping a `InterpolableList` of
// `InterpolableLength` objects.
// This class is templated in order to be able to handle
// column-rule-width/row-rule-width which are `GapDataList<int>` and also
// column-rule-color/row-rule-color which are `GapDataList<StyleColor>`.
template <typename T>
class CORE_EXPORT InterpolableGapDataRepeater : public InterpolableValue {
 public:
  InterpolableGapDataRepeater(InterpolableList* values,
                              ValueRepeater<T>* repeater)
      : values_(values), repeater_(repeater) {
    CHECK(values_);
    CHECK(repeater_);
  }

  // InterpolableValue implementation:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final {
    const InterpolableGapDataRepeater& gap_data_repeater_to =
        To<InterpolableGapDataRepeater>(to);
    InterpolableGapDataRepeater& gap_data_repeater_result =
        To<InterpolableGapDataRepeater>(result);
    values_->Interpolate(*gap_data_repeater_to.values_, progress,
                         *gap_data_repeater_result.values_);
  }

  bool Equals(const InterpolableValue& other) const final {
    return IsCompatibleWith(other) &&
           values_->Equals(*(To<InterpolableGapDataRepeater>(other).values_));
  }

  void Scale(double scale) final { values_->Scale(scale); }

  void Add(const InterpolableValue& other) final {
    DCHECK(IsCompatibleWith(other));
    values_->Add(*(To<InterpolableGapDataRepeater>(other).values_));
  }

  void AssertCanInterpolateWith(const InterpolableValue& other) const override =
      0;

  // Interpolable gap data repeaters are compatible when the lengths of the
  // values and the repeat count of their `ValueRepeater` are equal.
  virtual bool IsCompatibleWith(const InterpolableValue& other) const = 0;

  void Trace(Visitor* v) const override {
    InterpolableValue::Trace(v);
    v->Trace(values_);
    v->Trace(repeater_);
  }

  virtual GapData<T> CreateGapData(
      const CSSToLengthConversionData& conversion_data,
      Length::ValueRange value_range) const = 0;

 protected:
  Member<InterpolableList> values_;
  Member<ValueRepeater<T>> repeater_;
};

class InterpolableGapLengthRepeater final
    : public InterpolableGapDataRepeater<int> {
 public:
  InterpolableGapLengthRepeater(InterpolableList* values,
                                const ValueRepeater<int>* repeater)
      : InterpolableGapDataRepeater<int>(
            values,
            const_cast<ValueRepeater<int>*>(repeater)) {}

  static InterpolableGapLengthRepeater* Create(
      const ValueRepeater<int>* repeater,
      const CSSProperty& property,
      float zoom);

  bool IsGapLengthRepeater() const final { return true; }

  void AssertCanInterpolateWith(const InterpolableValue& other) const final;

  // Interpolable gap data repeaters are compatible when the lengths of the
  // values and the repeat count of their `ValueRepeater` are equal.
  bool IsCompatibleWith(const InterpolableValue& other) const override;

  GapData<int> CreateGapData(const CSSToLengthConversionData& conversion_data,
                             Length::ValueRange value_range) const final;

  static InterpolableValue* CreateItem(int value,
                                       const CSSProperty& property,
                                       float zoom) {
    return InterpolableLength::MaybeConvertLength(
        Length(value, Length::Type::kFixed), property, zoom, std::nullopt);
  }

 private:
  InterpolableGapLengthRepeater* RawClone() const final {
    InterpolableList* values(values_->Clone());
    return MakeGarbageCollected<InterpolableGapLengthRepeater>(values,
                                                               repeater_);
  }

  InterpolableGapLengthRepeater* RawCloneAndZero() const final {
    InterpolableList* values(values_->CloneAndZero());
    return MakeGarbageCollected<InterpolableGapLengthRepeater>(values,
                                                               repeater_);
  }
};

template <>
struct DowncastTraits<InterpolableGapLengthRepeater> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsGapLengthRepeater();
  }
};

template <>
struct DowncastTraits<InterpolableGapDataRepeater<int>> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsGapLengthRepeater();
  }
};

template <>
struct DowncastTraits<InterpolableGapDataRepeater<StyleColor>> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsGapColorRepeater();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_GAP_DATA_REPEATER_H_
