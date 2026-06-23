// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/base_interpolable_color.h"

#include "third_party/blink/renderer/core/animation/interpolable_color.h"
#include "third_party/blink/renderer/core/animation/interpolable_style_color.h"
#include "third_party/blink/renderer/core/animation/interpolation_value.h"

namespace blink {

/* static */
void BaseInterpolableColor::EnsureCompatible(InterpolationValue& start,
                                             InterpolationValue& end) {
  InterpolableColor* start_color =
      DynamicTo<InterpolableColor>(*start.interpolable_value);
  InterpolableColor* end_color =
      DynamicTo<InterpolableColor>(*end.interpolable_value);
  if (start_color && end_color) {
    InterpolableColor::SetupColorInterpolationSpaces(*start_color, *end_color);
    return;
  }
  // If one but not both endpoints are InterpolableColor, convert to
  // InterpolableStyleColor.
  if (start_color) {
    start.interpolable_value = InterpolableStyleColor::Create(start_color);
  }
  if (end_color) {
    end.interpolable_value = InterpolableStyleColor::Create(end_color);
  }
}

}  // namespace blink
