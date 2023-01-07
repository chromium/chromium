// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_INTERPOLATION_TYPES_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_INTERPOLATION_TYPES_MAP_H_

#include "third_party/blink/renderer/core/animation/interpolation_types_map.h"

namespace blink {

class SVGInterpolationTypesMap : public InterpolationTypesMap {
 public:
  SVGInterpolationTypesMap() = default;

  const InterpolationTypes& Get(const PropertyHandle&) const final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SVG_INTERPOLATION_TYPES_MAP_H_
