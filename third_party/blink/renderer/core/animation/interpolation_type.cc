// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolation_type.h"

#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"

namespace blink {

void InterpolationType::Composite(UnderlyingValueOwner& underlying_value_owner,
                                  double underlying_fraction,
                                  const InterpolationValue& value,
                                  double interpolation_fraction) const {
  DCHECK(!underlying_value_owner.Value().non_interpolable_value);
  DCHECK(!value.non_interpolable_value);
  underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
}

}  // namespace blink
