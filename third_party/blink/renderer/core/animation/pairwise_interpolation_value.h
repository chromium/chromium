// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PAIRWISE_INTERPOLATION_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PAIRWISE_INTERPOLATION_VALUE_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/animation/non_interpolable_value.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// Represents the smooth interpolation between an adjacent pair of
// PropertySpecificKeyframes.
struct PairwiseInterpolationValue {
  DISALLOW_NEW();

  PairwiseInterpolationValue(
      std::unique_ptr<InterpolableValue> start_interpolable_value,
      std::unique_ptr<InterpolableValue> end_interpolable_value,
      scoped_refptr<NonInterpolableValue> non_interpolable_value = nullptr)
      : start_interpolable_value(std::move(start_interpolable_value)),
        end_interpolable_value(std::move(end_interpolable_value)),
        non_interpolable_value(std::move(non_interpolable_value)) {}

  PairwiseInterpolationValue(std::nullptr_t) {}

  PairwiseInterpolationValue(PairwiseInterpolationValue&& other)
      : start_interpolable_value(std::move(other.start_interpolable_value)),
        end_interpolable_value(std::move(other.end_interpolable_value)),
        non_interpolable_value(std::move(other.non_interpolable_value)) {}

  operator bool() const { return start_interpolable_value.get(); }

  std::unique_ptr<InterpolableValue> start_interpolable_value;
  std::unique_ptr<InterpolableValue> end_interpolable_value;
  scoped_refptr<NonInterpolableValue> non_interpolable_value;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PAIRWISE_INTERPOLATION_VALUE_H_
