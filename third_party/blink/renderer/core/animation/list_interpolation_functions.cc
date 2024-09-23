// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"

#include <memory>
#include "base/functional/callback.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(NonInterpolableList);

const wtf_size_t kRepeatableListMaxLength = 1000;

// An UnderlyingValue used for compositing list items.
//
// When new NonInterpolableValues are set, the NonInterpolableList::AutoBuilder
// is modified at the corresponding index. The NonInterpolableValue of the
// underlying_list is updated when the AutoBuilder goes out of scope (if
// any calls to UnderlyingItemValue::SetNonInterpolableValue were made).
class UnderlyingItemValue : public UnderlyingValue {
  STACK_ALLOCATED();

 public:
  UnderlyingItemValue(UnderlyingValue& underlying_list,
                      NonInterpolableList::AutoBuilder& builder,
                      wtf_size_t index)
      : underlying_list_(underlying_list), builder_(builder), index_(index) {}

  InterpolableValue& MutableInterpolableValue() final {
    return *To<InterpolableList>(underlying_list_.MutableInterpolableValue())
                .GetMutable(index_);
  }
  void SetInterpolableValue(InterpolableValue* interpolable_value) final {
    To<InterpolableList>(underlying_list_.MutableInterpolableValue())
        .Set(index_, std::move(interpolable_value));
  }
  const NonInterpolableValue* GetNonInterpolableValue() const final {
    return To<NonInterpolableList>(*underlying_list_.GetNonInterpolableValue())
        .Get(index_);
  }
  void SetNonInterpolableValue(
      scoped_refptr<const NonInterpolableValue> non_interpolable_value) final {
    builder_.Set(index_, std::move(non_interpolable_value));
  }

 private:
  UnderlyingValue& underlying_list_;
  NonInterpolableList::AutoBuilder& builder_;
  wtf_size_t index_;
};

bool ListInterpolationFunctions::EqualValues(
    const InterpolationValue& a,
    const InterpolationValue& b,
    EqualNonInterpolableValuesCallback equal_non_interpolable_values) {
  if (!a && !b)
    return true;

  if (!a || !b)
    return false;

  const auto& interpolable_list_a = To<InterpolableList>(*a.interpolable_value);
  const auto& interpolable_list_b = To<InterpolableList>(*b.interpolable_value);

  if (interpolable_list_a.length() != interpolable_list_b.length())
    return false;

  wtf_size_t length = interpolable_list_a.length();
  if (length == 0)
    return true;

  const auto& non_interpolable_list_a =
      To<NonInterpolableList>(*a.non_interpolable_value);
  const auto& non_interpolable_list_b =
      To<NonInterpolableList>(*b.non_interpolable_value);

  for (wtf_size_t i = 0; i < length; i++) {
    if (!equal_non_interpolable_values(non_interpolable_list_a.Get(i),
                                       non_interpolable_list_b.Get(i)))
      return false;
  }
  return true;
}

static wtf_size_t MatchLengths(
    wtf_size_t start_length,
    wtf_size_t end_length,
    ListInterpolationFunctions::LengthMatchingStrategy
        length_matching_strategy) {
  if (length_matching_strategy ==
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual) {
    DCHECK_EQ(start_length, end_length);
    return start_length;
  } else if (length_matching_strategy ==
             ListInterpolationFunctions::LengthMatchingStrategy::
                 kLowestCommonMultiple) {
    // Combining the length expansion of lowestCommonMultiple with CSS
    // transitions has the potential to create pathological cases where this
    // algorithm compounds upon itself as the user starts transitions on already
    // animating values multiple times. This maximum limit is to avoid locking
    // up users' systems with memory consumption in the event that this occurs.
    // See crbug.com/739197 for more context.
    return std::min(kRepeatableListMaxLength,
                    static_cast<wtf_size_t>(
                        LowestCommonMultiple(start_length, end_length)));
  }
  DCHECK_EQ(length_matching_strategy,
            ListInterpolationFunctions::LengthMatchingStrategy::kPadToLargest);
  return std::max(start_length, end_length);
}

PairwiseInterpolationValue ListInterpolationFunctions::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end,
    LengthMatchingStrategy length_matching_strategy,
    MergeSingleItemConversionsCallback merge_single_item_conversions) {
  const wtf_size_t start_length =
      To<InterpolableList>(*start.interpolable_value).length();
  const wtf_size_t end_length =
      To<InterpolableList>(*end.interpolable_value).length();

  if (length_matching_strategy ==
          ListInterpolationFunctions::LengthMatchingStrategy::kEqual &&
      (start_length != end_length)) {
    return nullptr;
  }

  if (start_length == 0 && end_length == 0) {
    return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                      std::move(end.interpolable_value),
                                      nullptr);
  }

  if (start_length == 0) {
    InterpolableValue* start_interpolable_value =
        end.interpolable_value->CloneAndZero();
    return PairwiseInterpolationValue(start_interpolable_value,
                                      std::move(end.interpolable_value),
                                      std::move(end.non_interpolable_value));
  }

  if (end_length == 0) {
    InterpolableValue* end_interpolable_value =
        start.interpolable_value->CloneAndZero();
    return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                      end_interpolable_value,
                                      std::move(start.non_interpolable_value));
  }

  const wtf_size_t final_length =
      MatchLengths(start_length, end_length, length_matching_strategy);
  auto* result_start_interpolable_list =
      MakeGarbageCollected<InterpolableList>(final_length);
  auto* result_end_interpolable_list =
      MakeGarbageCollected<InterpolableList>(final_length);
  Vector<scoped_refptr<const NonInterpolableValue>>
      result_non_interpolable_values(final_length);

  auto& start_interpolable_list =
      To<InterpolableList>(*start.interpolable_value);
  auto& end_interpolable_list = To<InterpolableList>(*end.interpolable_value);
  const auto* start_non_interpolable_list =
      To<NonInterpolableList>(start.non_interpolable_value.get());
  const auto* end_non_interpolable_list =
      To<NonInterpolableList>(end.non_interpolable_value.get());
  const wtf_size_t start_non_interpolable_length =
      start_non_interpolable_list ? start_non_interpolable_list->length() : 0;
  const wtf_size_t end_non_interpolable_length =
      end_non_interpolable_list ? end_non_interpolable_list->length() : 0;
  for (wtf_size_t i = 0; i < final_length; i++) {
    if (length_matching_strategy ==
            LengthMatchingStrategy::kLowestCommonMultiple ||
        (i < start_length && i < end_length)) {
      InterpolationValue start_merge(
          start_interpolable_list.Get(i % start_length)->Clone(),
          start_non_interpolable_list ? start_non_interpolable_list->Get(
                                            i % start_non_interpolable_length)
                                      : nullptr);
      InterpolationValue end_merge(
          end_interpolable_list.Get(i % end_length)->Clone(),
          end_non_interpolable_list
              ? end_non_interpolable_list->Get(i % end_non_interpolable_length)
              : nullptr);
      PairwiseInterpolationValue result = merge_single_item_conversions(
          std::move(start_merge), std::move(end_merge));
      if (!result)
        return nullptr;
      result_start_interpolable_list->Set(
          i, std::move(result.start_interpolable_value));
      result_end_interpolable_list->Set(
          i, std::move(result.end_interpolable_value));
      result_non_interpolable_values[i] =
          std::move(result.non_interpolable_value);
    } else {
      DCHECK_EQ(length_matching_strategy,
                LengthMatchingStrategy::kPadToLargest);
      if (i < start_length) {
        result_start_interpolable_list->Set(
            i, start_interpolable_list.Get(i)->Clone());
        result_end_interpolable_list->Set(
            i, start_interpolable_list.Get(i)->CloneAndZero());
        result_non_interpolable_values[i] =
            (i < start_non_interpolable_length)
                ? start_non_interpolable_list->Get(i)
                : nullptr;
      } else {
        DCHECK_LT(i, end_length);
        result_start_interpolable_list->Set(
            i, end_interpolable_list.Get(i)->CloneAndZero());
        result_end_interpolable_list->Set(
            i, end_interpolable_list.Get(i)->Clone());
        result_non_interpolable_values[i] =
            (i < end_non_interpolable_length)
                ? end_non_interpolable_list->Get(i)
                : nullptr;
      }
    }
  }
  return PairwiseInterpolationValue(
      std::move(result_start_interpolable_list),
      std::move(result_end_interpolable_list),
      NonInterpolableList::Create(std::move(result_non_interpolable_values)));
}

static void RepeatToLength(InterpolationValue& value, wtf_size_t length) {
  auto& interpolable_list = To<InterpolableList>(*value.interpolable_value);
  const auto& non_interpolable_list =
      To<NonInterpolableList>(*value.non_interpolable_value);
  wtf_size_t current_length = interpolable_list.length();
  DCHECK_GT(current_length, 0U);
  if (current_length == length)
    return;
  DCHECK_LT(current_length, length);
  auto* new_interpolable_list = MakeGarbageCollected<InterpolableList>(length);
  Vector<scoped_refptr<const NonInterpolableValue>> new_non_interpolable_values(
      length);
  for (wtf_size_t i = length; i-- > 0;) {
    new_interpolable_list->Set(
        i, i < current_length
               ? std::move(interpolable_list.GetMutable(i).Get())
               : interpolable_list.Get(i % current_length)->Clone());
    new_non_interpolable_values[i] =
        non_interpolable_list.Get(i % current_length);
  }
  value.interpolable_value = std::move(new_interpolable_list);
  value.non_interpolable_value =
      NonInterpolableList::Create(std::move(new_non_interpolable_values));
}

// This helper function makes value the same length as length_value by
// CloneAndZero-ing the additional items from length_value into value.
static void PadToSameLength(InterpolationValue& value,
                            const InterpolationValue& length_value) {
  auto& interpolable_list = To<InterpolableList>(*value.interpolable_value);
  const auto& non_interpolable_list =
      To<NonInterpolableList>(*value.non_interpolable_value);
  const wtf_size_t current_length = interpolable_list.length();
  auto& target_interpolable_list =
      To<InterpolableList>(*length_value.interpolable_value);
  const auto& target_non_interpolable_list =
      To<NonInterpolableList>(*length_value.non_interpolable_value);
  const wtf_size_t target_length = target_interpolable_list.length();
  DCHECK_LT(current_length, target_length);
  auto* new_interpolable_list =
      MakeGarbageCollected<InterpolableList>(target_length);
  Vector<scoped_refptr<const NonInterpolableValue>> new_non_interpolable_values(
      target_length);
  wtf_size_t index = 0;
  for (; index < current_length; index++) {
    new_interpolable_list->Set(index,
                               std::move(interpolable_list.GetMutable(index)));
    new_non_interpolable_values[index] = non_interpolable_list.Get(index);
  }
  for (; index < target_length; index++) {
    new_interpolable_list->Set(
        index, target_interpolable_list.Get(index)->CloneAndZero());
    new_non_interpolable_values[index] =
        target_non_interpolable_list.Get(index);
  }
  value.interpolable_value = std::move(new_interpolable_list);
  value.non_interpolable_value =
      NonInterpolableList::Create(std::move(new_non_interpolable_values));
}

static bool InterpolableListsAreCompatible(
    const InterpolableList& a,
    const InterpolableList& b,
    wtf_size_t length,
    ListInterpolationFunctions::LengthMatchingStrategy length_matching_strategy,
    ListInterpolationFunctions::InterpolableValuesAreCompatibleCallback
        interpolable_values_are_compatible) {
  for (wtf_size_t i = 0; i < length; i++) {
    if (length_matching_strategy ==
            ListInterpolationFunctions::LengthMatchingStrategy::
                kLowestCommonMultiple ||
        (i < a.length() && i < b.length())) {
      if (!interpolable_values_are_compatible(a.Get(i % a.length()),
                                              b.Get(i % b.length()))) {
        return false;
      }
    }
  }
  return true;
}

static bool NonInterpolableListsAreCompatible(
    const NonInterpolableList& a,
    const NonInterpolableList& b,
    wtf_size_t length,
    ListInterpolationFunctions::LengthMatchingStrategy length_matching_strategy,
    ListInterpolationFunctions::NonInterpolableValuesAreCompatibleCallback
        non_interpolable_values_are_compatible) {
  for (wtf_size_t i = 0; i < length; i++) {
    if (length_matching_strategy ==
            ListInterpolationFunctions::LengthMatchingStrategy::
                kLowestCommonMultiple ||
        (i < a.length() && i < b.length())) {
      if (!non_interpolable_values_are_compatible(a.Get(i % a.length()),
                                                  b.Get(i % b.length()))) {
        return false;
      }
    }
  }
  return true;
}

bool ListInterpolationFunctions::VerifyNoNonInterpolableValues(
    const NonInterpolableValue* a,
    const NonInterpolableValue* b) {
  DCHECK(!a && !b);
  return true;
}

void ListInterpolationFunctions::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationType& type,
    const InterpolationValue& value,
    LengthMatchingStrategy length_matching_strategy,
    InterpolableValuesAreCompatibleCallback interpolable_values_are_compatible,
    NonInterpolableValuesAreCompatibleCallback
        non_interpolable_values_are_compatible,
    CompositeItemCallback composite_item) {
  const wtf_size_t underlying_length =
      To<InterpolableList>(*underlying_value_owner.Value().interpolable_value)
          .length();

  const auto& interpolable_list =
      To<InterpolableList>(*value.interpolable_value);
  const wtf_size_t value_length = interpolable_list.length();

  if (length_matching_strategy ==
          ListInterpolationFunctions::LengthMatchingStrategy::kEqual &&
      (underlying_length != value_length)) {
    underlying_value_owner.Set(type, value);
    return;
  }

  if (underlying_length == 0) {
    DCHECK(!underlying_value_owner.Value().non_interpolable_value);
    underlying_value_owner.Set(type, value);
    return;
  }

  if (value_length == 0) {
    DCHECK(!value.non_interpolable_value);
    underlying_value_owner.MutableValue().interpolable_value->Scale(
        underlying_fraction);
    return;
  }

  const wtf_size_t final_length =
      MatchLengths(underlying_length, value_length, length_matching_strategy);

  if (!InterpolableListsAreCompatible(
          To<InterpolableList>(
              *underlying_value_owner.Value().interpolable_value),
          interpolable_list, final_length, length_matching_strategy,
          interpolable_values_are_compatible)) {
    underlying_value_owner.Set(type, value);
    return;
  }

  const auto& non_interpolable_list =
      To<NonInterpolableList>(*value.non_interpolable_value);
  if (!NonInterpolableListsAreCompatible(
          To<NonInterpolableList>(
              *underlying_value_owner.Value().non_interpolable_value),
          non_interpolable_list, final_length, length_matching_strategy,
          non_interpolable_values_are_compatible)) {
    underlying_value_owner.Set(type, value);
    return;
  }

  InterpolationValue& underlying_value = underlying_value_owner.MutableValue();
  if (length_matching_strategy ==
      LengthMatchingStrategy::kLowestCommonMultiple) {
    if (underlying_length < final_length) {
      RepeatToLength(underlying_value, final_length);
    }
    NonInterpolableList::AutoBuilder builder(underlying_value_owner);

    for (wtf_size_t i = 0; i < final_length; i++) {
      UnderlyingItemValue underlying_item(underlying_value_owner, builder, i);
      composite_item(underlying_item, underlying_fraction,
                     *interpolable_list.Get(i % value_length),
                     non_interpolable_list.Get(i % value_length));
    }
  } else {
    DCHECK(length_matching_strategy == LengthMatchingStrategy::kPadToLargest ||
           length_matching_strategy == LengthMatchingStrategy::kEqual);
    if (underlying_length < final_length) {
      DCHECK_EQ(length_matching_strategy,
                LengthMatchingStrategy::kPadToLargest);
      DCHECK_EQ(value_length, final_length);
      PadToSameLength(underlying_value, value);
    }
    auto& underlying_interpolable_list =
        To<InterpolableList>(*underlying_value.interpolable_value);

    NonInterpolableList::AutoBuilder builder(underlying_value_owner);

    for (wtf_size_t i = 0; i < value_length; i++) {
      UnderlyingItemValue underlying_item(underlying_value_owner, builder, i);
      composite_item(underlying_item, underlying_fraction,
                     *interpolable_list.Get(i), non_interpolable_list.Get(i));
    }
    for (wtf_size_t i = value_length; i < final_length; i++) {
      underlying_interpolable_list.GetMutable(i)->Scale(underlying_fraction);
    }
  }
}

NonInterpolableList::AutoBuilder::AutoBuilder(UnderlyingValue& underlying_value)
    : underlying_value_(underlying_value) {
  DCHECK(underlying_value.GetNonInterpolableValue());
  DCHECK(IsA<NonInterpolableList>(underlying_value_.GetNonInterpolableValue()));
}

NonInterpolableList::AutoBuilder::~AutoBuilder() {
  // If no call to Set ever happened, there is no need to modify
  // underlying_value_.
  if (!list_.size())
    return;
  const auto& non_interpolable_list =
      To<NonInterpolableList>(*underlying_value_.GetNonInterpolableValue());
  DCHECK_EQ(non_interpolable_list.length(), list_.size());
  underlying_value_.SetNonInterpolableValue(
      NonInterpolableList::Create(std::move(list_)));
}

void NonInterpolableList::AutoBuilder::Set(
    wtf_size_t index,
    scoped_refptr<const NonInterpolableValue> non_interpolable_value) {
  // Copy list on first call to Set.
  if (!list_.size()) {
    const auto& non_interpolable_list =
        To<NonInterpolableList>(*underlying_value_.GetNonInterpolableValue());
    wtf_size_t underlying_length = non_interpolable_list.length();
    for (wtf_size_t i = 0; i < underlying_length; ++i)
      list_.push_back(non_interpolable_list.Get(i));
  }

  DCHECK_LT(index, list_.size());
  list_[index] = non_interpolable_value;
}

}  // namespace blink
