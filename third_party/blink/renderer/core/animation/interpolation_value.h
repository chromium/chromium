// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLATION_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLATION_VALUE_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/animation/non_interpolable_value.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// Represents a (non-strict) subset of a PropertySpecificKeyframe's value broken
// down into interpolable and non-interpolable parts. InterpolationValues can be
// composed together to represent a whole PropertySpecificKeyframe value.
struct InterpolationValue {
  DISALLOW_NEW();

  explicit InterpolationValue(
      std::unique_ptr<InterpolableValue> interpolable_value,
      scoped_refptr<const NonInterpolableValue> non_interpolable_value =
          nullptr)
      : interpolable_value(std::move(interpolable_value)),
        non_interpolable_value(std::move(non_interpolable_value)) {}

  InterpolationValue(std::nullptr_t) {}

  InterpolationValue(InterpolationValue&& other)
      : interpolable_value(std::move(other.interpolable_value)),
        non_interpolable_value(std::move(other.non_interpolable_value)) {}

  void operator=(InterpolationValue&& other) {
    interpolable_value = std::move(other.interpolable_value);
    non_interpolable_value = std::move(other.non_interpolable_value);
  }

  operator bool() const { return interpolable_value.get(); }

  InterpolationValue Clone() const {
    return InterpolationValue(
        interpolable_value ? interpolable_value->Clone() : nullptr,
        non_interpolable_value);
  }

  void Clear() {
    interpolable_value.reset();
    non_interpolable_value = nullptr;
  }

  std::unique_ptr<InterpolableValue> interpolable_value;
  scoped_refptr<const NonInterpolableValue> non_interpolable_value;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLATION_VALUE_H_
