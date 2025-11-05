// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_GAP_DATA_AUTO_REPEATER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_GAP_DATA_AUTO_REPEATER_H_

#include <memory>

#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/style/gap_data.h"

namespace blink {

class CSSProperty;
class StyleColor;
class StyleResolverState;

// This class is used to interpolate a `GapData` that is an auto repeater.
// Essentially, we represent the repeater by keeping a `InterpolableList` of
// `InterpolableLength` objects.
// This class is templated in order to be able to handle
// column-rule-width/row-rule-width which are `GapDataList<int>` and also
// column-rule-color/row-rule-color which are `GapDataList<StyleColor>`.
template <typename T>
class CORE_EXPORT InterpolableGapDataAutoRepeater : public InterpolableValue {
 public:
  InterpolableGapDataAutoRepeater(InterpolableList* values,
                                  const ValueRepeater<T>* repeater)
      : values_(values), repeater_(repeater) {
    CHECK(values_);
    CHECK(repeater_);
  }

  // InterpolableValue implementation:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final {
    const InterpolableGapDataAutoRepeater& gap_data_auto_repeater_to =
        To<InterpolableGapDataAutoRepeater>(to);
    InterpolableGapDataAutoRepeater& gap_data_auto_repeater_result =
        To<InterpolableGapDataAutoRepeater>(result);
    values_->Interpolate(*gap_data_auto_repeater_to.values_, progress,
                         *gap_data_auto_repeater_result.values_);
  }

  bool Equals(const InterpolableValue& other) const final {
    return IsCompatibleWith(other) &&
           values_->Equals(
               *(To<InterpolableGapDataAutoRepeater>(other).values_));
  }

  void Scale(double scale) final { values_->Scale(scale); }

  void Add(const InterpolableValue& other) final {
    DCHECK(IsCompatibleWith(other));
    values_->Add(*(To<InterpolableGapDataAutoRepeater>(other).values_));
  }

  void AssertCanInterpolateWith(const InterpolableValue& other) const override =
      0;

  // Interpolable gap data auto repeaters are compatible when they are both auto
  // repeaters.
  virtual bool IsCompatibleWith(const InterpolableValue& other) const = 0;

  void Trace(Visitor* v) const override {
    InterpolableValue::Trace(v);
    v->Trace(values_);
    v->Trace(repeater_);
  }

  InterpolableList* InnerValues() const { return values_; }

 protected:
  Member<InterpolableList> values_;
  const Member<const ValueRepeater<T>> repeater_;
};

class InterpolableGapLengthAutoRepeater final
    : public InterpolableGapDataAutoRepeater<int> {
 public:
  InterpolableGapLengthAutoRepeater(InterpolableList* values,
                                    const ValueRepeater<int>* repeater)
      : InterpolableGapDataAutoRepeater<int>(values, repeater) {}

  static InterpolableGapLengthAutoRepeater* Create(
      const ValueRepeater<int>* repeater,
      const CSSProperty& property,
      float zoom);

  bool IsGapLengthAutoRepeater() const final { return true; }

  void AssertCanInterpolateWith(const InterpolableValue& other) const final;

  // Interpolable gap data auto repeaters are compatible when they are both auto
  // repeaters.
  bool IsCompatibleWith(const InterpolableValue& other) const override;

  GapData<int> CreateGapData(const CSSToLengthConversionData& conversion_data,
                             Length::ValueRange value_range) const;

  static InterpolableValue* CreateItem(int value,
                                       const CSSProperty& property,
                                       float zoom) {
    return InterpolableLength::MaybeConvertLength(
        Length(value, Length::Type::kFixed), property, zoom, std::nullopt);
  }

  void Composite(const InterpolableGapLengthAutoRepeater& other,
                 double fraction);

  static InterpolableGapLengthAutoRepeater* CreateFromMergedInner(
      InterpolableList& merged_inner,
      const InterpolableGapLengthAutoRepeater& model) {
    // Note that `model.repeater_` may not match the values in `merged_inner`,
    // since the latter could have been expanded for kLowestCommonMultiple.
    return MakeGarbageCollected<InterpolableGapLengthAutoRepeater>(
        &merged_inner, model.repeater_);
  }

 private:
  InterpolableGapLengthAutoRepeater* RawClone() const final {
    InterpolableList* values(values_->Clone());
    return MakeGarbageCollected<InterpolableGapLengthAutoRepeater>(values,
                                                                   repeater_);
  }

  InterpolableGapLengthAutoRepeater* RawCloneAndZero() const final {
    InterpolableList* values(values_->CloneAndZero());
    return MakeGarbageCollected<InterpolableGapLengthAutoRepeater>(values,
                                                                   repeater_);
  }
};

template <>
struct DowncastTraits<InterpolableGapLengthAutoRepeater> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsGapLengthAutoRepeater();
  }
};

class InterpolableGapColorAutoRepeater final
    : public InterpolableGapDataAutoRepeater<StyleColor> {
 public:
  InterpolableGapColorAutoRepeater(InterpolableList* values,
                                   const ValueRepeater<StyleColor>* repeater)
      : InterpolableGapDataAutoRepeater<StyleColor>(values, repeater) {}

  static InterpolableGapColorAutoRepeater* Create(
      const ValueRepeater<StyleColor>* repeater,
      const ComputedStyle& style);

  bool IsGapColorAutoRepeater() const final { return true; }

  void AssertCanInterpolateWith(const InterpolableValue& other) const final;

  // Interpolable gap data auto repeaters are compatible when they are both auto
  // repeaters.
  bool IsCompatibleWith(const InterpolableValue& other) const override;

  GapData<StyleColor> CreateGapData(StyleResolverState& state) const;

  static InterpolableValue* CreateItem(const StyleColor& value,
                                       const ComputedStyle& style);

  void Composite(const InterpolableGapColorAutoRepeater& other,
                 double fraction);

  static InterpolableGapColorAutoRepeater* CreateFromMergedInner(
      InterpolableList& merged_inner,
      const InterpolableGapColorAutoRepeater& model) {
    // Note that `model.repeater_` may not match the values in `merged_inner`,
    // since the latter could have been expanded for kLowestCommonMultiple.
    return MakeGarbageCollected<InterpolableGapColorAutoRepeater>(
        &merged_inner, model.repeater_);
  }

 private:
  InterpolableGapColorAutoRepeater* RawClone() const final {
    InterpolableList* values(values_->Clone());
    return MakeGarbageCollected<InterpolableGapColorAutoRepeater>(values,
                                                                  repeater_);
  }

  InterpolableGapColorAutoRepeater* RawCloneAndZero() const final {
    InterpolableList* values(values_->CloneAndZero());
    return MakeGarbageCollected<InterpolableGapColorAutoRepeater>(values,
                                                                  repeater_);
  }
};

template <>
struct DowncastTraits<InterpolableGapColorAutoRepeater> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsGapColorAutoRepeater();
  }
};

template <>
struct DowncastTraits<InterpolableGapDataAutoRepeater<int>> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsGapLengthAutoRepeater();
  }
};

template <>
struct DowncastTraits<InterpolableGapDataAutoRepeater<StyleColor>> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsGapColorAutoRepeater();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_GAP_DATA_AUTO_REPEATER_H_
