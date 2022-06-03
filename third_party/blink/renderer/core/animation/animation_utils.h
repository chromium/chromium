// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_UTILS_H_

#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/animation/keyframe.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

class CORE_EXPORT AnimationUtils {
  STATIC_ONLY(AnimationUtils);

 public:
  static const CSSValue* KeyframeValueFromComputedStyle(const PropertyHandle&,
                                                        const ComputedStyle&,
                                                        const Document&,
                                                        const LayoutObject*);

  // Resolves the value of each property in properties, based on the underlying
  // value including any animation effects included in the interpolations map.
  // A callback is triggered for each property with the resolved value.
  static void ForEachInterpolatedPropertyValue(
      Element* target,
      const PropertyHandleSet& properties,
      ActiveInterpolationsMap& interpolations,
      base::RepeatingCallback<void(PropertyHandle, const CSSValue*)> callback);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_UTILS_H_
