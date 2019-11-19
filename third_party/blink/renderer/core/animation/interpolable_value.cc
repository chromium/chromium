// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_value.h"

#include <memory>

namespace blink {

bool InterpolableNumber::Equals(const InterpolableValue& other) const {
  return value_ == ToInterpolableNumber(other).value_;
}

bool InterpolableList::Equals(const InterpolableValue& other) const {
  const InterpolableList& other_list = ToInterpolableList(other);
  if (length() != other_list.length())
    return false;
  for (wtf_size_t i = 0; i < length(); i++) {
    if (!values_[i]->Equals(*other_list.values_[i]))
      return false;
  }
  return true;
}

void InterpolableNumber::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  DCHECK(other.IsNumber());
}

void InterpolableNumber::Interpolate(const InterpolableValue& to,
                                     const double progress,
                                     InterpolableValue& result) const {
  const InterpolableNumber& to_number = ToInterpolableNumber(to);
  InterpolableNumber& result_number = ToInterpolableNumber(result);

  if (progress == 0 || value_ == to_number.value_)
    result_number.value_ = value_;
  else if (progress == 1)
    result_number.value_ = to_number.value_;
  else
    result_number.value_ =
        value_ * (1 - progress) + to_number.value_ * progress;
}

void InterpolableList::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  DCHECK(other.IsList());
  DCHECK_EQ(ToInterpolableList(other).length(), length());
}

void InterpolableList::Interpolate(const InterpolableValue& to,
                                   const double progress,
                                   InterpolableValue& result) const {
  const InterpolableList& to_list = ToInterpolableList(to);
  InterpolableList& result_list = ToInterpolableList(result);

  for (wtf_size_t i = 0; i < length(); i++) {
    DCHECK(values_[i]);
    DCHECK(to_list.values_[i]);
    values_[i]->Interpolate(*(to_list.values_[i]), progress,
                            *(result_list.values_[i]));
  }
}

InterpolableList* InterpolableList::RawCloneAndZero() const {
  auto* result = new InterpolableList(length());
  for (wtf_size_t i = 0; i < length(); i++)
    result->Set(i, values_[i]->CloneAndZero());
  return result;
}

void InterpolableNumber::Scale(double scale) {
  value_ = value_ * scale;
}

void InterpolableList::Scale(double scale) {
  for (wtf_size_t i = 0; i < length(); i++)
    values_[i]->Scale(scale);
}

void InterpolableNumber::Add(const InterpolableValue& other) {
  value_ += ToInterpolableNumber(other).value_;
}

void InterpolableList::Add(const InterpolableValue& other) {
  const InterpolableList& other_list = ToInterpolableList(other);
  DCHECK_EQ(other_list.length(), length());
  for (wtf_size_t i = 0; i < length(); i++)
    values_[i]->Add(*other_list.values_[i]);
}

void InterpolableList::ScaleAndAdd(double scale,
                                   const InterpolableValue& other) {
  const InterpolableList& other_list = ToInterpolableList(other);
  DCHECK_EQ(other_list.length(), length());
  for (wtf_size_t i = 0; i < length(); i++)
    values_[i]->ScaleAndAdd(scale, *other_list.values_[i]);
}

}  // namespace blink
