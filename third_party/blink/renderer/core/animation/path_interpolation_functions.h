// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PATH_INTERPOLATION_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PATH_INTERPOLATION_FUNCTIONS_H_

#include <optional>

#include "third_party/blink/renderer/core/animation/interpolation_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class StylePath;

class CORE_EXPORT PathInterpolationFunctions {
  STATIC_ONLY(PathInterpolationFunctions);

 public:
  enum CoordinateConversion { kPreserveCoordinates, kForceAbsolute };

  static StylePath* AppliedValue(const InterpolableValue&,
                                 const NonInterpolableValue&);

  static void Composite(UnderlyingValueOwner&,
                        double underlying_fraction,
                        const InterpolationType*,
                        const InterpolationValue&);

  static InterpolationValue ConvertValue(
      const StylePath*,
      CoordinateConversion,
      std::optional<ShapeBox> css_box = std::nullopt);

  static InterpolationValue MaybeConvertNeutral(
      const InterpolationValue& underlying,
      InterpolationType::ConversionCheckers&);

  static bool PathsAreCompatible(const NonInterpolableValue& start,
                                 const NonInterpolableValue& end);

  static bool IsPathNonInterpolableValue(const NonInterpolableValue& value);

  // Returns the <shape-box> stored on the non-interpolable value for
  // shape-outside path() animations. Other callers leave this unset.
  static std::optional<ShapeBox> GetCssBox(const NonInterpolableValue&);

  static PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PATH_INTERPOLATION_FUNCTIONS_H_
