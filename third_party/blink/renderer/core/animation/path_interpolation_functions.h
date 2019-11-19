// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PATH_INTERPOLATION_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PATH_INTERPOLATION_FUNCTIONS_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolation_type.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class StylePath;

class PathInterpolationFunctions {
  STATIC_ONLY(PathInterpolationFunctions);

 public:
  enum CoordinateConversion { PreserveCoordinates, ForceAbsolute };

  static std::unique_ptr<SVGPathByteStream> AppliedValue(
      const InterpolableValue&,
      const NonInterpolableValue*);

  static void Composite(UnderlyingValueOwner&,
                        double underlying_fraction,
                        const InterpolationType&,
                        const InterpolationValue&);

  static InterpolationValue ConvertValue(const SVGPathByteStream&,
                                         CoordinateConversion);

  static InterpolationValue ConvertValue(const StylePath*,
                                         CoordinateConversion);

  static InterpolationValue MaybeConvertNeutral(
      const InterpolationValue& underlying,
      InterpolationType::ConversionCheckers&);

  static PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PATH_INTERPOLATION_FUNCTIONS_H_
