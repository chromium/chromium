// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_CURVE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_CURVE_H_

#include <memory>

#include "third_party/blink/renderer/platform/platform_export.h"

namespace cc {
class AnimationCurve;
}

namespace blink {

class PLATFORM_EXPORT CompositorAnimationCurve {
 public:
  virtual ~CompositorAnimationCurve() = default;
  virtual std::unique_ptr<cc::AnimationCurve> CloneToAnimationCurve() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_CURVE_H_
